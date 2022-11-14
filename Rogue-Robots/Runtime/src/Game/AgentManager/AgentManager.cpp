#include "AgentManager.h"
#include "AgentBehaviorSystems.h"
#include "Game/GameLayer.h"

using namespace DOG;
using namespace DirectX::SimpleMath;


AgentManager AgentManager::s_amInstance;
bool AgentManager::s_notInitialized = true;

/*******************************
		Public Methods
*******************************/


entity AgentManager::CreateAgent(EntityTypes type, u32 groupID, const Vector3& pos)
{
	entity e = CreateAgentCore(GetModel(type), groupID, pos, type);
	EntityManager& em = EntityManager::Get();

	em.AddComponent<AgentSeekPlayerComponent>(e);
	em.AddComponent<NetworkAgentStats>(e);
	// Add CreateAndDestroyEntityComponent to ECS
	if (GameLayer::GetNetworkStatus() == NetworkStatus::Hosting)
	{
		AgentIdComponent& agent = em.GetComponent<AgentIdComponent>(e);
		TransformComponent& agentTrans = em.GetComponent<TransformComponent>(e);
		
		CreateAndDestroyEntityComponent& create = em.AddComponent<CreateAndDestroyEntityComponent>(e);
		create.alive = true;
		create.entityTypeId = agent.type;
		create.id = agent.id;
		create.position = agentTrans.GetPosition();
		em.Collect<ThisPlayer, NetworkPlayerComponent>().Do(
			[&](ThisPlayer&, NetworkPlayerComponent& net) { create.playerId = net.playerId; });
	}
	return e;
}


void AgentManager::CreateOrDestroyShadowAgent(CreateAndDestroyEntityComponent& entityDesc)
{
	if (entityDesc.alive)
	{
		entity e = CreateAgentCore(GetModel(entityDesc), GroupID(entityDesc.id), entityDesc.position, entityDesc.entityTypeId);

		EntityManager::Get().AddComponent<ShadowAgentSeekPlayerComponent>(e);
	}
	else
	{
		EntityManager::Get().Collect<AgentIdComponent, TransformComponent>().Do(
			[&](entity e, AgentIdComponent& agent, TransformComponent& trans)
			{
				if (agent.id == entityDesc.id)
				{
					trans.SetPosition(entityDesc.position);
					DestroyLocalAgent(e);
					trans.SetPosition(entityDesc.position);
				}
			}
		);
		
	}
}

u32 AgentManager::GenAgentID(u32 groupID)
{
	u32 agentID = (m_agentIdCounter[groupID] << GROUP_BITS) | groupID;
	++m_agentIdCounter[groupID];
	return agentID;
}

void AgentManager::CountAgentKilled(u32 agentID)
{
	u32 i = GroupID(agentID);
	++m_agentKillCounter[i];

	if (m_agentKillCounter[i] == m_agentIdCounter[i])
		m_agentKillCounter[i] = m_agentIdCounter[i] = 0;	// reset both counters
}

u32 AgentManager::GroupID(u32 agentID)
{
	if (agentID == NULL_AGENT)
	{
		// find first empty group
		for (u32 i = 0; i < m_agentIdCounter.size(); ++i)
		{
			if (m_agentIdCounter[i] == 0)
			{
				ASSERT(i < m_agentIdCounter.size(), "found no empty agent group");
				return i;
			}
		}
	}

	// return embedded groupID
	return agentID & MASK;
}

/*******************************
		Private Methods
*******************************/

AgentManager::AgentManager() noexcept
{

}


void AgentManager::Initialize()
{
	// Load (all) agent model asset(s)
	s_amInstance.m_models.push_back(AssetManager::Get().LoadModelAsset("Assets/Models/Enemies/enemy1.gltf"));

	// Register agent systems
	EntityManager& em = EntityManager::Get();
	em.RegisterSystem(std::make_unique<AgentSeekPlayerSystem>());
	em.RegisterSystem(std::make_unique<AgentMovementSystem>());
	em.RegisterSystem(std::make_unique<AgentAttackSystem>());
	em.RegisterSystem(std::make_unique<AgentHitDetectionSystem>());
	em.RegisterSystem(std::make_unique<AgentHitSystem>());
	em.RegisterSystem(std::make_unique<AgentAggroSystem>());
	em.RegisterSystem(std::make_unique<AgentFrostTimerSystem>());
	em.RegisterSystem(std::make_unique<AgentDestructSystem>());

	// Register shadow agent systems
	em.RegisterSystem(std::make_unique<ShadowAgentSeekPlayerSystem>());

	// Register late update agent systems
	em.RegisterSystem(std::make_unique<LateAgentDestructCleanupSystem>());

	// Set status to initialized
	s_notInitialized = false;
}

entity AgentManager::CreateAgentCore(u32 model, u32 groupID, const Vector3& pos, EntityTypes type)
{
	EntityManager& em = EntityManager::Get();
	entity e = em.CreateEntity();

	// Set default components
	TransformComponent& trans = em.AddComponent<TransformComponent>(e);
	trans.SetPosition(pos);

	em.AddComponent<ModelComponent>(e, model);

	em.AddComponent<CapsuleColliderComponent>(e, e, 0.25f, 0.25f, true, 50.0f);
	
	RigidbodyComponent& rb = em.AddComponent<RigidbodyComponent>(e, e);
	rb.ConstrainRotation(true, true, true);
	rb.disableDeactivation = true;
	rb.getControlOfTransform = true;
	
	AgentIdComponent& agent = em.AddComponent<AgentIdComponent>(e);
	agent.id = GenAgentID(groupID);
	agent.type = type;

	em.Collect<ThisPlayer, NetworkPlayerComponent>().Do(
		[&](ThisPlayer&, NetworkPlayerComponent& player)
		{
			if (player.playerId == 0)
			{
				AgentMovementComponent& move = em.AddComponent<AgentMovementComponent>(e);
				move.station = pos + GenerateRandomVector3(agent.id);
				move.forward = move.station - pos;
				move.forward.Normalize();
			}
		});
	em.AddComponent<AgentPathfinderComponent>(e);

	em.AddComponent<AgentHPComponent>(e);

	// Add networking components
	if (GameLayer::GetNetworkStatus() != NetworkStatus::Offline)
	{
		em.AddComponent<NetworkTransform>(e).objectId = agent.id;
	}

	// Should this component exist on ALL agents or is it only related to networking?
	if (!em.HasComponent<ShadowReceiverComponent>(e))
		em.AddComponent<ShadowReceiverComponent>(e);

	return e;
}

u32 AgentManager::GetModel(EntityTypes type)
{
	return m_models[static_cast<u32>(type) - static_cast<u32>(EntityTypes::AgentsBegin)];
}

u32 AgentManager::GetModel(CreateAndDestroyEntityComponent& entityDesc)
{
	return m_models[static_cast<u32>(entityDesc.entityTypeId) - static_cast<u32>(EntityTypes::AgentsBegin)];
}

Vector3 AgentManager::GenerateRandomVector3(u32 seed, f32 max, f32 min)
{
	//static std::random_device rdev;
	//static std::mt19937 gen(rdev());
	static std::mt19937 gen(seed);
	static std::uniform_real_distribution<f32> udis(min, max);
	return Vector3(udis(gen), udis(gen), udis(gen));
}

void AgentManager::DestroyLocalAgent(entity e)
{
	EntityManager& em = EntityManager::Get();

	AgentIdComponent& agent = em.GetComponent<AgentIdComponent>(e);
	TransformComponent& agentTrans = em.GetComponent<TransformComponent>(e);
	//ModelComponent& agentModel = em.GetComponent<ModelComponent>(e);

	u32 bodyID = AssetManager::Get().LoadModelAsset("Assets/Models/Enemies/SplitUpEnemy/Body.gltf", DOG::AssetLoadFlag::GPUMemory);
	entity corpseBody = em.CreateEntity();
	em.AddComponent<TransformComponent>(corpseBody).worldMatrix = agentTrans.worldMatrix;
	em.AddComponent<ModelComponent>(corpseBody, bodyID);
	em.AddComponent<SphereColliderComponent>(corpseBody, corpseBody, 0.5f, true);
	em.AddComponent<RigidbodyComponent>(corpseBody, corpseBody);

	u32 tailID = AssetManager::Get().LoadModelAsset("Assets/Models/Enemies/SplitUpEnemy/Tail.gltf", DOG::AssetLoadFlag::GPUMemory);
	entity corpseTail = em.CreateEntity();
	em.AddComponent<TransformComponent>(corpseTail).worldMatrix = agentTrans.worldMatrix;
	em.AddComponent<ModelComponent>(corpseTail, tailID);
	em.AddComponent<SphereColliderComponent>(corpseTail, corpseTail, 0.5f, true);
	em.AddComponent<RigidbodyComponent>(corpseTail, corpseTail);

	u32 legID = AssetManager::Get().LoadModelAsset("Assets/Models/Enemies/SplitUpEnemy/Leg1.gltf", DOG::AssetLoadFlag::GPUMemory);
	const u32 legsAmount = 6;
	const u32 legsOnEachSide = 3;
	const f32 legsDistanceFromEachOther = 0.20168f;

	for (int i = 0; i < legsAmount; ++i)
	{
		entity corpseLeg = em.CreateEntity();
		TransformComponent& legTrans = em.AddComponent<TransformComponent>(corpseLeg);
		legTrans.worldMatrix = agentTrans.worldMatrix;

		legTrans.SetPosition(legTrans.GetPosition() + legTrans.GetForward() * float(i % legsOnEachSide) * legsDistanceFromEachOther);
		if (i >= legsOnEachSide)
			legTrans.RotateL(Vector3(0.0f, DirectX::XM_PI, 0.0f));

		em.AddComponent<ModelComponent>(corpseLeg, legID);
		em.AddComponent<SphereColliderComponent>(corpseLeg, corpseLeg, 0.5f, true);
		em.AddComponent<RigidbodyComponent>(corpseLeg, corpseLeg);
	}

	entity corpse = em.CreateEntity();
	em.AddComponent<AgentCorpse>(corpse);
	//Leaving this here for safety reasons
	//em.AddComponent<ModelComponent>(corpse, agentModel.id);
	//TransformComponent& corpseTrans = em.AddComponent<TransformComponent>(corpse);
	//corpseTrans = agentTrans;
	//corpseTrans.SetRotation(Vector3(-2, 0, -2));

	CreateAndDestroyEntityComponent& kill = em.AddComponent<CreateAndDestroyEntityComponent>(corpse);
	kill.alive = false;
	kill.entityTypeId = agent.type;
	kill.id = agent.id;
	em.Collect<ThisPlayer, NetworkPlayerComponent>().Do(
		[&](ThisPlayer&, NetworkPlayerComponent& net) { kill.playerId = net.playerId; });
	kill.position = agentTrans.GetPosition();

	em.DeferredEntityDestruction(e);
}
