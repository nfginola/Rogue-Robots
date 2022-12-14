#include "PrefabInstantiatorFunctions.h"
#include "GameComponent.h"

using namespace DOG;
using namespace DirectX;
using namespace DirectX::SimpleMath;

std::vector<DOG::entity> SpawnPlayers(const Vector3& pos, u8 playerCount, f32 spread)
{
	ASSERT(playerCount > 0, "Need to at least spawn ThisPlayer. I.e. playerCount has to exceed 0");

	auto* scriptManager = LuaMain::GetScriptManager();
	//// Add persistent material prefab lua
	//{
	//	entity e = m_entityManager.CreateEntity();
	//	m_entityManager.AddComponent<TransformComponent>(e);
	//	scriptManager->AddScript(e, "MaterialPrefabs.lua");
	//}

	//LuaMain::GetGlobal().
	scriptManager->RunLuaFile("MaterialPrefabs.lua");
	LuaMain::GetGlobal()->GetTable("MaterialPrefabs").CallFunctionOnTable("OnStart");

	auto& am = AssetManager::Get();
	auto& em = EntityManager::Get();
	std::array<u32, 4> playerModels = {};
	playerModels[0] = am.LoadModelAsset("Assets/Models/P2/Red/testAim.gltf");
	playerModels[1] = am.LoadModelAsset("Assets/Models/P2/Blue/testAim.gltf");
	playerModels[2] = am.LoadModelAsset("Assets/Models/P2/Green/testAim.gltf");
	playerModels[3] = am.LoadModelAsset("Assets/Models/P2/Yellow/testAim.gltf");

	std::array<DirectX::SimpleMath::Vector3, 4> playerOutlineColors
	{
		DirectX::SimpleMath::Vector3{ 1.f, 0.f, 0.f },
		DirectX::SimpleMath::Vector3{ 0.f, 0.f, 1.f},
		DirectX::SimpleMath::Vector3{ 0.f, 1.f, 0.f},
		DirectX::SimpleMath::Vector3{ 1.f, 1.f, 0.f},
	};
	
	std::vector<entity> players;
	for (auto i = 0; i < playerCount; ++i)
	{
		entity playerI = players.emplace_back(em.CreateEntity());
		Vector3 offset = {
			spread * (i % 2) - (spread / 2.f),
			0,
			spread * (i / 2) - (spread / 2.f),
		};
		em.AddComponent<TransformComponent>(playerI, pos - offset);
		em.AddComponent<CapsuleColliderComponent>(playerI, playerI, 0.25f, 1.35f, true, 75.f);
		auto& rb = em.AddComponent<RigidbodyComponent>(playerI, playerI);
		rb.ConstrainRotation(true, true, true);
		rb.disableDeactivation = true;
		rb.getControlOfTransform = true;
		rb.setGravityForRigidbody = true;
		//Set the gravity for the player to 2.5g
		rb.gravityForRigidbody = Vector3(0.0f, -25.0f, 0.0f);

		em.AddComponent<PlayerStatsComponent>(playerI);
		em.AddComponent<PlayerControllerComponent>(playerI);
		auto& npc = em.AddComponent<NetworkPlayerComponent>(playerI);
		npc.playerId = static_cast<i8>(i);
		npc.playerName = i == 0 ? "Red" : i == 1 ? "Blue" : i == 2 ? "Green" : "Yellow";

		em.AddComponent<InputController>(playerI);
		em.AddComponent<PlayerAliveComponent>(playerI);
		em.AddComponent<AnimationComponent>(playerI);
		em.AddComponent<AudioComponent>(playerI);
		em.AddComponent<MixamoHeadJointTF>(playerI);
		em.AddComponent<MixamoRightHandJointTF>(playerI);

		auto& ac = em.GetComponent<AnimationComponent>(playerI);
		ac.animatorID = static_cast<i8>(i);
		ac.SimpleAdd(static_cast<i8>(MixamoAnimations::Idle), AnimationFlag::Looping | AnimationFlag::ResetPrio);

		auto& bc = em.AddComponent<BarrelComponent>(playerI);
		bc.type = BarrelComponent::Type::Bullet;
		bc.maximumAmmoCapacityForType = 999'999;
		bc.ammoPerPickup = 30;
		bc.currentAmmoCount = 30;

		em.AddComponent<MagazineModificationComponent>(playerI).type = MagazineModificationComponent::Type::None;
		em.AddComponent<MiscComponent>(playerI).type = MiscComponent::Type::Basic;

		entity playerModelEntity = em.CreateEntity();

		em.AddComponent<TransformComponent>(playerModelEntity);
		em.AddComponent<ModelComponent>(playerModelEntity, playerModels[i]);
		em.AddComponent<RigDataComponent>(playerModelEntity);
		em.AddComponent<ShadowReceiverComponent>(playerModelEntity);
		em.AddComponent<OutlineComponent>(playerModelEntity, playerOutlineColors[i]);


		auto& rc = em.GetComponent<RigDataComponent>(playerModelEntity);
		rc.offset = i * MIXAMO_RIG.nJoints;

		auto& t = em.AddComponent<ChildComponent>(playerModelEntity);
		t.parent = playerI;
		t.localTransform.SetPosition({0.0f, -0.5f, 0.0f});

		if (i == 0) // Only for this player
		{
			em.AddComponent<DontDraw>(playerModelEntity);
			em.AddComponent<ThisPlayer>(playerI);
			em.AddComponent<AudioListenerComponent>(playerI);
		}
		else
		{
			em.AddComponent<OnlinePlayer>(playerI);
		}
		// Always add outline component
		em.AddComponent<OutlineComponent>(playerI, playerOutlineColors[i]);

		//scriptManager->AddScript(playerI, "Gun.lua");
		//scriptManager->AddScript(playerI, "PassiveItemSystem.lua");
		//scriptManager->AddScript(playerI, "ActiveItemSystem.lua");
	}

	return players;
}


std::vector<entity> AddFlashlightsToPlayers(const std::vector<entity>& players)
{
	auto& em = EntityManager::Get();
	std::vector<entity> flashlights;
	for (auto i = 0; i < players.size(); ++i)
	{
		entity flashLightEntity = em.CreateEntity();

		em.AddComponent<DOG::TransformComponent>(flashLightEntity);

		ChildToBoneComponent& childComponent = em.AddComponent<ChildToBoneComponent>(flashLightEntity);
		childComponent.boneParent = players[i];
		childComponent.localTransform.SetPosition(Vector3(90.f, 130.f, -45.f));
			
		auto& tc = childComponent.localTransform;

		auto up = tc.worldMatrix.Up();
		up.Normalize();

		auto& cc = em.AddComponent<DOG::CameraComponent>(flashLightEntity);
		cc.isMainCamera = false;
		cc.viewMatrix = DirectX::XMMatrixLookAtLH
		(
			{ tc.GetPosition().x, tc.GetPosition().y, tc.GetPosition().z },
			{ tc.GetPosition().x + tc.GetForward().x, tc.GetPosition().y + tc.GetForward().y, tc.GetPosition().z + tc.GetForward().z },
			{ up.x, up.y, up.z }
		);

		auto dd = DOG::SpotLightDesc();
		dd.color = { 1.0f, 1.0f, 1.0f };
		dd.direction = tc.GetForward();
		dd.strength = 0.6f;
		dd.cutoffAngle = 33.0f;

		auto lh = DOG::LightManager::Get().AddSpotLight(dd, DOG::LightUpdateFrequency::PerFrame);

		auto& slc = em.AddComponent<DOG::SpotLightComponent>(flashLightEntity);
		slc.color = dd.color;
		slc.direction = tc.GetForward();
		slc.strength = dd.strength;
		slc.cutoffAngle = dd.cutoffAngle;
		slc.handle = lh;
		slc.owningPlayer = players[i];

		float fov = ((slc.cutoffAngle + 0.1f) * 2.0f) * DirectX::XM_PI / 180.f;
		cc.projMatrix = DirectX::XMMatrixPerspectiveFovLH(fov, 1, 800.f, 0.1f);

#if defined NDEBUG
		em.AddComponent<DOG::ShadowCasterComponent>(flashLightEntity);
#endif
		if (i == 0) // Only for this/main player
			slc.isMainPlayerSpotlight = true;
		else
			slc.isMainPlayerSpotlight = false;

		flashlights.push_back(flashLightEntity);
	}
	return flashlights;
}

std::vector<entity> AddGunsToPlayers(const std::vector<entity>& players)
{
	auto& em = EntityManager::Get();
	auto& am = AssetManager::Get();
	
	const std::array<DirectX::SimpleMath::Vector3, 4> playerOutlineColors
	{
		DirectX::SimpleMath::Vector3{ 1.f, 0.f, 0.f },
		DirectX::SimpleMath::Vector3{ 0.f, 0.f, 1.f},
		DirectX::SimpleMath::Vector3{ 0.f, 1.f, 0.f},
		DirectX::SimpleMath::Vector3{ 1.f, 1.f, 0.f},
	};

	std::vector<entity> guns;
	for (auto i = 0; i < players.size(); ++i)
	{
		entity gunEntity = em.CreateEntity();

		em.AddComponent<DOG::TransformComponent>(gunEntity);
		em.AddComponent<ModelComponent>(gunEntity, am.LoadModelAsset("Assets/Models/ModularRifle/Maingun.gltf"));
		em.AddComponent<ShadowReceiverComponent>(gunEntity);
		em.AddComponent<OutlineComponent>(gunEntity, playerOutlineColors[i]);

		auto& childComponent = em.AddComponent<ChildToBoneComponent>(gunEntity);
		childComponent.boneParent = players[i];

		const auto translation = XMMatrixTranslation(73, 117, -45);
		const auto rotation = XMMatrixRotationRollPitchYaw(4.f * XM_PI/180.f, 190.f * XM_PI / 180.f, 91.f * XM_PI / 180.f);
		const auto scaling = XMMatrixScaling(200, 120.f, 145.f);
		const auto gunBoneSpaceOffset = scaling * rotation * translation;

		childComponent.localTransform.worldMatrix = Matrix(gunBoneSpaceOffset);

		if (em.HasComponent<ThisPlayer>(childComponent.boneParent))
		{
			em.AddComponent<DontDraw>(gunEntity);
		}

		guns.push_back(gunEntity);
	}
	return guns;
}

entity SpawnTurretProjectile(const DirectX::SimpleMath::Matrix& transform, float speed, float dmg, float lifeTime, DOG::entity turret, DOG::entity owner)
{
	constexpr float radius = 0.1f;
	static bool init = true;
	static SubmeshRenderer projectileModel;
	if (init)
	{
		auto shapeCreator = ShapeCreator(Shape::sphere, 16, 16, radius);
		auto shape = shapeCreator.GetResult();
		MeshDesc mesh;
		mesh.indices = shape->mesh.indices;
		mesh.submeshData = shape->submeshes;
		for (auto& [attr, vert] : shape->mesh.vertexData)
		{
			mesh.vertexDataPerAttribute[attr] = vert;
		}
		projectileModel.mesh = CustomMeshManager::Get().AddMesh(mesh).first;
		projectileModel.materialDesc.emissiveFactor = { 1.6f, 0.8f, 0.002f, 1 };
		projectileModel.materialDesc.albedoFactor = { 0.2f, 0.2f, 0.2f, 1 };
		projectileModel.material = CustomMaterialManager::Get().AddMaterial(projectileModel.materialDesc);
		init = false;
	}

	auto& em = EntityManager::Get();

	entity p = em.CreateEntity();
	if (auto scene = em.TryGetComponent<SceneComponent>(turret); scene) em.AddComponent<SceneComponent>(p, scene->get().scene);

	auto& pTransform = em.AddComponent<TransformComponent>(p);
	pTransform.worldMatrix = transform;
	em.AddComponent<SubmeshRenderer>(p) = projectileModel;

	em.AddComponent<SphereColliderComponent>(p, p, radius, true, 0.1f);
	auto& rb = em.AddComponent<RigidbodyComponent>(p, p);
	rb.continuousCollisionDetection = true;
	rb.linearVelocity = speed * pTransform.GetForward();




	LightHandle pointLight = LightManager::Get().AddPointLight(PointLightDesc(), LightUpdateFrequency::PerFrame);
	em.AddComponent<PointLightComponent>(p, pointLight, Vector3(1, 0.5f, 0.001f), 5.f);


	em.AddComponent<TurretProjectileComponent>(p).maxLifeTime = lifeTime;
	auto& bullet = em.AddComponent<BulletComponent>(p);
	bullet.damage = dmg;
	bullet.playerEntityID = owner;

	return p;
}

DOG::entity SpawnLaserBlob(const DOG::TransformComponent& transform, DOG::entity owner) noexcept
{
	auto& em = EntityManager::Get();
	

	static std::optional<SubmeshRenderer> laserModel = std::nullopt;
	if (!laserModel)
	{
		MaterialDesc matDesc;
		matDesc.emissiveFactor = 6 * Vector4(1.5f, 0.1f, 0.1f, 0);
		matDesc.albedoFactor = { 0.5f, 0, 0, 1 };
		TransformComponent matrix;
		matrix.RotateW({ XM_PIDIV2, 0, 0 }).SetScale(Vector3(0.08f, 0.6f, 0.08f));
		laserModel = CreateSimpleModel(matDesc, ShapeCreator(Shape::prism, 16, 8).GetResult()->mesh, matrix);
	}

	entity laser = em.CreateEntity();
	em.AddComponent<TransformComponent>(laser) = transform;
	em.AddComponent<SubmeshRenderer>(laser) = *laserModel;

	if (em.Exists(owner))
	{
		if (auto scene = em.TryGetComponent<SceneComponent>(owner); scene) em.AddComponent<SceneComponent>(laser, scene->get().scene);
	}



	return laser;
}

DOG::entity SpawnGlowStick(const TransformComponent& transform, DOG::entity owner) noexcept
{
	auto& em = EntityManager::Get();

	entity glowStick = em.CreateEntity();
	auto& tr = em.AddComponent<TransformComponent>(glowStick);
	tr = transform;
	if (em.Exists(owner))
	{
		if (auto scene = em.TryGetComponent<SceneComponent>(owner); scene) em.AddComponent<SceneComponent>(glowStick, scene->get().scene);
	}

	Vector3 scale = 0.1f * Vector3(1, 4, 1);
	static std::optional<SubmeshRenderer> glowSticModel = std::nullopt;
	if (!glowSticModel)
	{
		MaterialDesc matDesc;
		matDesc.emissiveFactor = 2 * Vector4(0.12f, 1.0f, 0.12f, 0.0f);
		matDesc.albedoFactor = { 0, 0.5f, 0, 1 };
		TransformComponent matrix;
		matrix.SetScale(scale);
		glowSticModel = CreateSimpleModel(matDesc, ShapeCreator(Shape::prism, 16, 8).GetResult()->mesh, matrix);
	}

	em.AddComponent<SubmeshRenderer>(glowStick) = *glowSticModel;

	em.AddComponent<BoxColliderComponent>(glowStick, glowStick, 1.2f * scale, true, 0.1f);
	auto& rb = em.AddComponent<RigidbodyComponent>(glowStick, glowStick);
	rb.continuousCollisionDetection = true;

	em.AddComponent<GlowStickComponent>(glowStick).spawnTime = static_cast<f32>(Time::ElapsedTime());

	LightHandle pointLight = LightManager::Get().AddPointLight(PointLightDesc(), LightUpdateFrequency::PerFrame);
	auto& light = em.AddComponent<PointLightComponent>(glowStick, pointLight);
	light.color = { glowSticModel->materialDesc.emissiveFactor.x, 0.6f * glowSticModel->materialDesc.emissiveFactor.y, glowSticModel->materialDesc.emissiveFactor.z };
	light.radius = 12.0f;
	light.strength = 0.6f;
	return glowStick;
}
