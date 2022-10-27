#pragma once
#include <DOGEngine.h>

struct LuaVector3
{
	f32 x, y, z;

	explicit LuaVector3(DOG::LuaTable& table);
	static DOG::LuaTable Create(DirectX::SimpleMath::Vector3 vec);
};


class LuaInterface
{
public:
	LuaInterface() noexcept = default;
	virtual ~LuaInterface() noexcept = default;
};

class InputInterface : public LuaInterface
{
public:
	InputInterface() noexcept
	{

	}
	~InputInterface() noexcept
	{
	
	}

	void IsLeftPressed(DOG::LuaContext* context);

	void IsRightPressed(DOG::LuaContext* context);

	//Takes a string as argument.
	void IsKeyPressed(DOG::LuaContext* context);

	void GetMouseDelta(DOG::LuaContext* context);
};

class SceneInterface : public LuaInterface
{
public:
	void CreateEntity(DOG::LuaContext* context);
};

class EntityInterface : public LuaInterface
{
public:
	EntityInterface() noexcept
	{

	}
	~EntityInterface() noexcept
	{

	}

	void CreateEntity(DOG::LuaContext* context);

	void DestroyEntity(DOG::LuaContext* context);

	//Takes an entity-ID as input.
	//Also takes a string that tells what component to add.
	//The user also has to send the appropriate input for that component.
	void AddComponent(DOG::LuaContext* context);

	//Takes an entity-ID as input.
	//Also takes a string that tells what component to change.
	//The user also has to send the appropriate input for that component.
	void ModifyComponent(DOG::LuaContext* context);

	//Takes an entity-ID as input.
	void GetTransformPosData(DOG::LuaContext* context);
	void GetTransformScaleData(DOG::LuaContext* context);

	void SetRotationForwardUp(DOG::LuaContext* context);

	void GetPlayerStats(DOG::LuaContext* context);
	void GetPlayerStat(DOG::LuaContext* context);

	void SetPlayerStats(DOG::LuaContext* context);
	void SetPlayerStat(DOG::LuaContext* context);
	
	void GetForward(DOG::LuaContext* context);

	void GetUp(DOG::LuaContext* context);
	
	void GetRight(DOG::LuaContext* context);

	void GetPlayerControllerCamera(DOG::LuaContext* context);

	void GetAction(DOG::LuaContext* context);
	
	void SetAction(DOG::LuaContext* context);

	void HasComponent(DOG::LuaContext* context);

	void PlayAudio(DOG::LuaContext* context);

	void GetPassiveType(DOG::LuaContext* context);
	
	void IsBulletLocal(DOG::LuaContext* context);

	void Exists(DOG::LuaContext* context);

	//void AgentHit(DOG::LuaContext* context);

	void ModifyAnimationComponent(DOG::LuaContext* context);

private:
	void AddModel(DOG::LuaContext* context, DOG::entity e);

	void AddTransform(DOG::LuaContext* context, DOG::entity e);

	void AddNetwork(DOG::entity e);

	void AddAgentStats(DOG::LuaContext* context, DOG::entity e);

	void AddAudio(DOG::LuaContext* context, DOG::entity e);

	void AddBoxCollider(DOG::LuaContext* context, DOG::entity e);

	void AddBoxColliderMass(DOG::LuaContext* context, DOG::entity e);

	void AddSphereCollider(DOG::LuaContext* context, DOG::entity e);

	void AddSphereTrigger(DOG::LuaContext* context, DOG::entity e);

	void AddRigidbody(DOG::LuaContext* context, DOG::entity e);

	void AddBullet(DOG::LuaContext* context, DOG::entity e);
	
	void AddSubmeshRender(DOG::LuaContext* context, DOG::entity e);

	void AddScript(DOG::LuaContext* context, DOG::entity e);


	void AddHomingMissile(DOG::LuaContext* context, DOG::entity e);

	void ModifyTransform(DOG::LuaContext* context, DOG::entity e);
	
	void ModifyPlayerStats(DOG::LuaContext* context, DOG::entity e);

	void ModifyPointLightStrength(DOG::LuaContext* context, DOG::entity e);
};

class AssetInterface : public LuaInterface
{
public:
	AssetInterface() noexcept
	{

	}
	~AssetInterface() noexcept
	{

	}

	//Takes a string, path to the model.
	void LoadModel(DOG::LuaContext* context);
	void LoadAudio(DOG::LuaContext* context);

private:

};

class HostInterface : public LuaInterface
{
public:
	HostInterface() noexcept
	{

	}
	~HostInterface() noexcept
	{

	}
	
	void DistanceToPlayers(DOG::LuaContext* context);
};

class PhysicsInterface : public LuaInterface
{
public:
	PhysicsInterface() noexcept {};
	~PhysicsInterface() noexcept {};

	void RBSetVelocity(DOG::LuaContext* context);
	void Explosion(DOG::LuaContext* context);
	void RBConstrainRotation(DOG::LuaContext* context);
	void RBConstrainPosition(DOG::LuaContext* context);
};

class RenderInterface : public LuaInterface
{
public:
	RenderInterface() noexcept {};
	~RenderInterface() noexcept {};

	void CreateMaterial(DOG::LuaContext* context);
};

class GameInterface : public LuaInterface
{
public:
	GameInterface() noexcept {};
	~GameInterface() noexcept {};

	void ExplosionEffect(DOG::LuaContext* context);
	void AmmoUI(DOG::LuaContext* context);
};
