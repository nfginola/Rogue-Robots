#pragma once
#include <DOGEngine.h>

enum class AgentBehavior
{
	Default,
	MoveTo,
	Attack,
	PopBehaviorFromStack = Default,
};

struct AgentStatsComponent
{
	float hp;
	float maxHP;
	float speed;
	//...

	u32 roomId;
};

struct AgentIdComponent
{
	u32 id;
};

struct AgentMovementComponent
{
	f32 baseSpeed = 15.0f;
	f32 currentSpeed = baseSpeed;
	DirectX::SimpleMath::Vector3 forward{1, 0, 0};
};

struct AgentHPComponent
{
	f32 hp = 100.0f;
	f32 maxHP = 100.0f;
};

struct AgentAttackComponent
{
	f32 radiusSquared = 1.5f;
	f32 damage = 10.0f;
	f32 coolDown = 1.0f;
	f32 elapsedTime = coolDown;
};

struct AgentPathfinderComponent
{
	DirectX::SimpleMath::Vector3 targetPos;
	// TODO: PlannedPath
};

struct AgentSeekPlayerComponent
{
	i8 playerID = -1;
	DOG::entity entityID = 0;
	DirectX::SimpleMath::Vector3 direction;
	f32 squaredDistance = 0.0f;
};

struct AgentHitComponent
{
	struct Hit
	{
		DOG::entity entityHitBy{ DOG::NULL_ENTITY };
		f32 damage{ 0.0f };
		i8 playerNetworkID{ -1 };
	};
	i8 count = 0;
	std::array<Hit, 20> hits;
	void HitBy(DOG::entity e, i8 playerNetworkID) { hits[count++ % hits.max_size()] = {e, 25.0f, playerNetworkID }; } // Temporary default dmg for bullets that don't know how much dmg they want to do.
	void HitBy(Hit hit) { hits[count++ % hits.max_size()] = hit; }
};

/*******************************************

			Network Components
 
********************************************/

struct NetworkAgentStats
{
	i32 playerId;
	u32 objectId;
	AgentStatsComponent stats;
};

struct NetworkAgentSeekPlayer
{
	i8 playerID = -1;
	u32 agentID = u32(-1);
};
