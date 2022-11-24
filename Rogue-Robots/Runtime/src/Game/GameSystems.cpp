#include "GameSystems.h"

using namespace DOG;
using namespace DirectX;
using namespace SimpleMath;

#pragma region PlayerMovementSystem

void PlayerMovementSystem::OnEarlyUpdate(
	Entity e,
	PlayerControllerComponent& player,
	PlayerStatsComponent& playerStats,
	TransformComponent& transform,
	RigidbodyComponent& rigidbody,
	InputController& input,
	AnimationComponent& ac)
{
	if (input.toggleMoveView)
	{
		input.toggleMoveView = false;
		player.moveView = !player.moveView;
	}

	// Create a new camera entity for this player
	if (player.cameraEntity == DOG::NULL_ENTITY)
	{
		player.cameraEntity = CreatePlayerCameraEntity(e);
	}

	CameraComponent& camera = EntityManager::Get().GetComponent<CameraComponent>(player.cameraEntity);
	TransformComponent& cameraTransform = EntityManager::Get().GetComponent<TransformComponent>(player.cameraEntity);

	// Set the main camera to be ThisPlayer's camera
	bool isThisPlayer = false;
	if (EntityManager::Get().HasComponent<ThisPlayer>(e))
	{
		isThisPlayer = true;
		camera.isMainCamera = true;
	}

	if (input.toggleDebug && EntityManager::Get().HasComponent<PlayerAliveComponent>(e))
	{
		input.toggleDebug = false;
		if (player.debugCamera == DOG::NULL_ENTITY)
		{
			player.debugCamera = CreateDebugCamera(e);
			EntityManager::Get().GetComponent<TransformComponent>(player.debugCamera).worldMatrix = cameraTransform;
			auto& debugCamera = EntityManager::Get().GetComponent<CameraComponent>(player.debugCamera);
			debugCamera.isMainCamera = true;
		}
		else
		{
			EntityManager::Get().DeferredEntityDestruction(player.debugCamera);
			player.debugCamera = DOG::NULL_ENTITY;
		}
	}

	// Rotate player
	Vector3 forward = cameraTransform.GetForward();
	Vector3 right(1, 0, 0);
	if (player.moveView && isThisPlayer)
	{
		forward = GetNewForward(player);
	}
	right = s_globUp.Cross(forward);

	// Move player
	auto moveTowards = GetMoveTowards(input, forward, right);

	if (player.debugCamera != DOG::NULL_ENTITY && isThisPlayer)
	{
		camera.isMainCamera = false;
		MoveDebugCamera(player.debugCamera, moveTowards, forward, right, 10.f, input);
		return;
	}

	MovePlayer(e, player, moveTowards, forward, rigidbody, playerStats.speed, input, ac);
	ApplyAnimations(input, ac);

	f32 aspectRatio = (f32)Window::GetWidth() / Window::GetHeight();
	camera.projMatrix = XMMatrixPerspectiveFovLH(80.f * XM_PI / 180.f, aspectRatio, 800.f, 0.1f);

	// Place camera 0.8 units above the player transform
	auto pos = transform.GetPosition() + Vector3(0, 0.7f, 0);
	camera.viewMatrix = XMMatrixLookToLH(pos, forward, forward.Cross(right));
	cameraTransform.worldMatrix = camera.viewMatrix.Invert();

	// Update the player transform rotation around the Y-axis to match the camera's
	auto camForward = cameraTransform.GetForward();
	camForward.y = 0;
	camForward.Normalize();

	auto prevScale = transform.GetScale();
	transform.worldMatrix = XMMatrixLookToLH(transform.GetPosition(), camForward, s_globUp);
	transform.worldMatrix = transform.worldMatrix.Invert();
	transform.SetScale(prevScale);
}

PlayerMovementSystem::Entity PlayerMovementSystem::CreateDebugCamera(Entity e) noexcept
{
	Entity debugCamera = EntityManager::Get().CreateEntity();
	if (EntityManager::Get().HasComponent<SceneComponent>(e))
	{
		EntityManager::Get().AddComponent<SceneComponent>(debugCamera,
			EntityManager::Get().GetComponent<SceneComponent>(e).scene);
	}

	EntityManager::Get().AddComponent<TransformComponent>(debugCamera);
	EntityManager::Get().AddComponent<CameraComponent>(debugCamera);

	return debugCamera;
}

PlayerMovementSystem::Entity PlayerMovementSystem::CreatePlayerCameraEntity(Entity player) noexcept
{
	Entity playerCamera = EntityManager::Get().CreateEntity();
	if (EntityManager::Get().HasComponent<SceneComponent>(player))
	{
		EntityManager::Get().AddComponent<SceneComponent>(playerCamera,
			EntityManager::Get().GetComponent<SceneComponent>(player).scene);
	}

	EntityManager::Get().AddComponent<TransformComponent>(playerCamera).SetScale(Vector3(1, 1, 1));
	EntityManager::Get().AddComponent<CameraComponent>(playerCamera);

	return playerCamera;
}

Vector3 PlayerMovementSystem::GetNewForward(PlayerControllerComponent& player) const noexcept
{
	auto [mouseX, mouseY] = DOG::Mouse::GetDeltaCoordinates();

	player.azimuthal -= mouseX * player.mouseSensitivity * XM_2PI;
	player.polar += mouseY * player.mouseSensitivity * XM_2PI;

	player.polar = std::clamp(player.polar, 0.0001f, XM_PI - 0.0001f);
	Vector3 forward = XMVectorSet(
		std::cos(player.azimuthal) * std::sin(player.polar),
		std::cos(player.polar),
		std::sin(player.azimuthal) * std::sin(player.polar),
		0
	);

	return forward;
}

Vector3 PlayerMovementSystem::GetMoveTowards(const InputController& input, Vector3 forward, Vector3 right) const noexcept
{
	Vector3 xzForward = forward;
	xzForward.y = 0;
	xzForward.Normalize();

	Vector3 moveTowards = Vector3(0, 0, 0);

	auto forwardBack = input.forward - input.backwards;
	auto leftRight = input.right - input.left;

	moveTowards = (f32)forwardBack * xzForward + (f32)leftRight * right;

	moveTowards.Normalize();

	return moveTowards;
}

void PlayerMovementSystem::MoveDebugCamera(Entity e, Vector3 moveTowards, Vector3 forward, Vector3 right, f32 speed, const InputController& input) noexcept
{
	auto& transform = EntityManager::Get().GetComponent<TransformComponent>(e);
	auto& camera = EntityManager::Get().GetComponent<CameraComponent>(e);
	camera.isMainCamera = true;

	transform.SetPosition((transform.GetPosition() += moveTowards * speed * (f32)Time::DeltaTime()));

	if (input.up)
		transform.SetPosition(transform.GetPosition() += s_globUp * speed * (f32)Time::DeltaTime());

	if (input.down)
		transform.SetPosition(transform.GetPosition() -= s_globUp * speed * (f32)Time::DeltaTime());

	f32 aspectRatio = (f32)Window::GetWidth() / Window::GetHeight();
	camera.projMatrix = XMMatrixPerspectiveFovLH(80.f * XM_PI / 180.f, aspectRatio, 800.f, 0.1f);

	auto pos = transform.GetPosition();
	camera.viewMatrix = XMMatrixLookToLH(pos, forward, forward.Cross(right));
}

void PlayerMovementSystem::MovePlayer(Entity, PlayerControllerComponent& player, Vector3 moveTowards, Vector3 forward,
	RigidbodyComponent& rb, f32 speed, InputController& input, AnimationComponent& ac)
{	
	auto forwardDisparity = moveTowards.Dot(forward);
	speed = forwardDisparity < -0.01f ? speed / 2.f : speed;

	rb.linearVelocity = Vector3(
		moveTowards.x * speed,
		rb.linearVelocity.y,
		moveTowards.z * speed
	);

	if (input.up && !player.jumping)
	{
		player.jumping = true;
		rb.linearVelocity.y = 6.f;
		// Simple jump animation, not really fitting with current jump movement
		{
			static constexpr i8 JUMP_ANIMATION_ID = 4;
			auto& setter = ac.animSetters[ac.addedSetters];
			setter.animationIDs[0] = JUMP_ANIMATION_ID;
			setter.targetWeights[0] = 1.f;
			setter.playbackRate = 0.5f; // we jump high. lower playbackrate fits better
			setter.transitionLength = 0.5f;
			setter.loop = false;
			++ac.addedSetters;
		}
	}
}

void PlayerMovementSystem::ApplyAnimations(const InputController& input, AnimationComponent& ac)
{
	// Relevant Animation IDs
	static constexpr i8 IDLE = 0;
	static constexpr i8 RUN = 5;
	static constexpr i8 RUN_BACKWARDS = 6;
	static constexpr i8 WALK = 13;
	static constexpr i8 WALK_BACKWARDS = 14;
	static constexpr i8 STRAFE_LEFT = 8;
	static constexpr i8 STRAFE_RIGHT = 2;
	//static constexpr i8 STRAFE_RIGHT = 10;

	auto addedAnims = 0;
	auto& setter = ac.animSetters[ac.addedSetters];
	setter.group = ac.FULL_BODY;

	auto forwardBack = input.forward - input.backwards;
	auto leftRight = input.right - input.left;
	if (forwardBack)
	{
		const auto animation = input.forward ? RUN : WALK_BACKWARDS;
		const auto weight = 0.5f;

		setter.animationIDs[addedAnims] = animation;
		setter.targetWeights[addedAnims++] = weight;
	}
	if (leftRight)
	{
		const auto animation = input.left ? STRAFE_LEFT : STRAFE_RIGHT;

		// Backwards + strafe_right makes leg clip through each other if equal weights
		auto weight = (forwardBack && input.backwards && input.right) ? 0.7f : 0.5f;

		setter.animationIDs[addedAnims] = animation;
		setter.targetWeights[addedAnims++] = weight;
	}

	// if no schmovement apply idle animation
	if (!addedAnims)
	{
		ac.SimpleAdd(2, AnimationFlag::Looping);
		/*setter.animationIDs[addedAnims] = IDLE;
		setter.targetWeights[addedAnims++] = 1.0f;*/
	}
	else
	{
		setter.flag = AnimationFlag::Looping;
		// misc variables
		setter.playbackRate = 1.5f;
		setter.transitionLength = 0.1f;
		setter.loop = true;
		++ac.addedSetters;
	}
	
}

#pragma endregion



void UpdateParentNode(entity parent)
{
	auto& em = EntityManager::Get();
	assert(em.HasComponent<TransformComponent>(parent));
	if (auto parentAsChild = em.TryGetComponent<ChildComponent>(parent); parentAsChild && !parentAsChild->get().nodeHasBeenUpdated)
	{
		entity grandParent = parentAsChild->get().parent;
		if (em.Exists(grandParent)) // Grand parent might have been removed. In this case we ignore it.
		{
			UpdateParentNode(grandParent);
			auto& grandParentWorld = em.GetComponent<TransformComponent>(grandParent);
			auto& parentWorld = em.GetComponent<TransformComponent>(parent);
			parentWorld.worldMatrix = parentAsChild->get().localTransform * grandParentWorld.worldMatrix;
			parentAsChild->get().nodeHasBeenUpdated = true;
		}
	}
}

void ScuffedSceneGraphSystem::OnUpdate(entity e, ChildComponent& child, TransformComponent& world)
{
	auto& em = EntityManager::Get();
	if (em.Exists(child.parent))
	{
		if (!child.nodeHasBeenUpdated)
		{
			UpdateParentNode(child.parent);
			auto& parentWorld = em.GetComponent<TransformComponent>(child.parent);
			world.worldMatrix = child.localTransform * parentWorld.worldMatrix;
			child.nodeHasBeenUpdated = true;
		}
	}
	else
	{
		em.DeferredEntityDestruction(e);
	}
}


void ScuffedSceneGraphSystem::OnLateUpdate(ChildComponent& child)
{
	child.nodeHasBeenUpdated = false;
}
void DespawnSystem::OnUpdate(DOG::entity e, DespawnComponent& despawn)
{
	if (despawn.despawnTimer < Time::ElapsedTime())
	{
		EntityManager::Get().DeferredEntityDestruction(e);
		EntityManager::Get().RemoveComponent<DespawnComponent>(e);
	}
}

#pragma endregion
