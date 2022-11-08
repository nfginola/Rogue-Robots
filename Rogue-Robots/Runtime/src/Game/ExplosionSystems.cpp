#include "ExplosionSystems.h"

#include "../DOGEngine/src/Core/CustomMaterialManager.h"

using Vector3 = DirectX::SimpleMath::Vector3;
using namespace DOG;
void ExplosionSystem::OnUpdate(entity e, TransformComponent& explosionTransform, ExplosionComponent& explosionInfo)
{
	Vector3 explosionPosition = explosionTransform.GetPosition();

	float power = explosionInfo.power;
	float radius = explosionInfo.radius;

	EntityManager::Get().Collect<TransformComponent, RigidbodyComponent>().Do([&](TransformComponent& transform, RigidbodyComponent& rigidbody)
		{
			Vector3 position = transform.GetPosition();
			float distance = Vector3::Distance(position, explosionPosition);
			if (distance > radius)
				return;

			//float squaredDistance = Vector3::DistanceSquared(position, explosionPosition);
			//if (squaredDistance < 1.0f)
			//	squaredDistance = 1.0f;
			//power /= squaredDistance;

			Vector3 direction = (position - explosionPosition);
			direction.Normalize();
			rigidbody.centralImpulse = direction * rigidbody.mass * power;
		});

	EntityManager::Get().RemoveComponent<ExplosionComponent>(e);
}

u32 ExplosionEffectSystem::explosionEffectModelID = 0; 
entity ExplosionEffectSystem::CreateExplosionEffect(entity parentEntity, float radius, float growTime, float shrinkTime)
{
	if (explosionEffectModelID == 0)
	{
		constexpr const char* modelName = "Assets/Models/Temporary_Assets/Explosion.glb";
		explosionEffectModelID = AssetManager::Get().LoadModelAsset(modelName);
	}

	entity newEntity = EntityManager::Get().CreateEntity();
	if (EntityManager::Get().HasComponent<SceneComponent>(parentEntity))
		EntityManager::Get().AddComponent<SceneComponent>(newEntity, EntityManager::Get().GetComponent<SceneComponent>(parentEntity).scene);

	Vector3 parentPosition = EntityManager::Get().GetComponent<TransformComponent>(parentEntity).GetPosition();

	EntityManager::Get().AddComponent<TransformComponent>(newEntity, parentPosition, Vector3(.0f, .0f, .0f), Vector3(radius, radius, radius));
	EntityManager::Get().AddComponent<ModelComponent>(newEntity, explosionEffectModelID);

	//Set values for explosions on the script
	LuaMain::GetScriptManager()->AddScript(newEntity, "ExplosionEffect.lua");
	if (growTime != -1.0f || shrinkTime != -1.0f)
	{
		ScriptData scriptData = LuaMain::GetScriptManager()->GetScript(newEntity, "ExplosionEffect.lua");
		LuaTable scriptTable(scriptData.scriptTable, true);

		if (growTime != -1.0f)
			scriptTable.AddFloatToTable("growTime", growTime);
		if (shrinkTime != -1.0f)
			scriptTable.AddFloatToTable("shrinkTime", shrinkTime);
	}

	AddEffectsToExplosion(parentEntity, newEntity);

	return newEntity;
}

void ExplosionEffectSystem::AddEffectsToExplosion(DOG::entity parentEntity, DOG::entity explosionEntity)
{
	std::string materialName = "";
	//Base explosion color
	Vector3 color = { 0.8f, 0.0f, 0.0f };

	if (EntityManager::Get().HasComponent<FrostEffectComponent>(parentEntity))
	{	
		materialName = "FrostExplosionMaterial";
		color = { 0.2f, 0.6f, 0.8f };
	}

	if (!materialName.empty() && EntityManager::Get().HasComponent<ModelComponent>(explosionEntity))
	{
		LuaTable materialPrefabsTable = LuaMain::GetGlobal()->GetTable("MaterialPrefabs");
		LuaTable materialTable(materialPrefabsTable.CallFunctionOnTable("GetMaterial", materialName).table);

		ModelComponent& model = EntityManager::Get().GetComponent<ModelComponent>(explosionEntity);

		ModelAsset* modelAsset = AssetManager::Get().GetAsset<ModelAsset>(model.id);

		if (!modelAsset)
		{
			std::cout << "Model has not been loaded in yet! Model ID: " << model.id << "\n";
			return;
		}

		MaterialHandle materialHandle;
		materialHandle.handle = static_cast<u64>(materialTable.GetIntFromTable("materialHandle"));

		LuaTable albedoFactor = materialTable.GetTableFromTable("albedoFactor");

		MaterialDesc materialDesc{};
		materialDesc.albedoFactor = { albedoFactor.GetFloatFromTable("x"), albedoFactor.GetFloatFromTable("y"), albedoFactor.GetFloatFromTable("z") };
		materialDesc.roughnessFactor = (float)materialTable.GetDoubleFromTable("roughnessFactor");
		materialDesc.metallicFactor = (float)materialTable.GetDoubleFromTable("metallicFactor");

		EntityManager::Get().AddComponent<SubmeshRenderer>(explosionEntity, modelAsset->gfxModel->mesh.mesh, materialHandle, materialDesc);
		EntityManager::Get().RemoveComponent<ModelComponent>(explosionEntity);
	}

	// Add dynamic point light
	auto pdesc = PointLightDesc();
	pdesc.color = color;
	pdesc.strength = 100.f;
	auto& plc = EntityManager::Get().AddComponent<PointLightComponent>(explosionEntity);
	plc.handle = LightManager::Get().AddPointLight(pdesc, LightUpdateFrequency::PerFrame);
	plc.color = pdesc.color;
	plc.strength = pdesc.strength;
}

void ExplosionEffectSystem::OnUpdate(DOG::entity e, ExplosionEffectComponent& explosionInfo)
{
	CreateExplosionEffect(e, explosionInfo.radius, explosionInfo.growTime, explosionInfo.shrinkTime);

	EntityManager::Get().RemoveComponent<ExplosionEffectComponent>(e);
}