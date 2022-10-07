#include "PhysicsEngine.h"
#pragma warning(push, 0)
#include "BulletPhysics/btBulletDynamicsCommon.h"
#pragma warning(pop)
#include "../ECS/EntityManager.h"
#include "../Core/AssetManager.h"
#include "../Scripting/LuaMain.h"

namespace DOG
{
	PhysicsEngine PhysicsEngine::s_physicsEngine;

	struct BulletCollisionCallback : public btCollisionWorld::ContactResultCallback {

		BulletCollisionCallback() : btCollisionWorld::ContactResultCallback() {}

		//Keep this code here in case we need it for later
		//virtual bool needsCollision(btBroadphaseProxy* proxy) const {
		//	// superclass will check m_collisionFilterGroup and m_collisionFilterMask
		//	if (!btCollisionWorld::ContactResultCallback::needsCollision(proxy))
		//		return false;
		//	// if passed filters, may also want to avoid contacts between constraints
		//	return PhysicsEngine::GetRigidbodyColliderData(rigidbodyHandle)->rigidBody->checkCollideWithOverride(static_cast<btCollisionObject*>(proxy->m_clientObject));
		//}

		virtual btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper* colObj0Wrap, int, int,
			const btCollisionObjectWrapper* colObj1Wrap, int, int)
		{
			//Get rigidobdy handles
			const u32 byteShift = 4;
			u64 obj0RigidbodyHandle = (colObj0Wrap->getCollisionObject()->getUserIndex2() << byteShift) | colObj0Wrap->getCollisionObject()->getUserIndex();
			u64 obj1RigidbodyHandle = (colObj1Wrap->getCollisionObject()->getUserIndex2() << byteShift) | colObj1Wrap->getCollisionObject()->getUserIndex();

			//Get handle for obj0
			u32 obj0handle = PhysicsEngine::s_physicsEngine.m_handleAllocator.GetSlot(obj0RigidbodyHandle);

			//Get the collisionKeeper for obj0
			auto collisions = PhysicsEngine::s_physicsEngine.m_rigidbodyCollision.find(obj0handle);

			//Get handle for obj1
			u32 obj1Handle = PhysicsEngine::s_physicsEngine.m_handleAllocator.GetSlot(obj1RigidbodyHandle);

			//Check if obj1 exist in the collisionKeeper if it does then we set the collisionCheck true
			auto obj1Collision = collisions->second.find(obj1Handle);
			if (obj1Collision != collisions->second.end())
			{
				obj1Collision->second.collisionCheck = true;
			}
			else
			{
				//Add new collision to the collisionKeeper
				RigidbodyCollisionData collisionData;
				collisionData.activeCollision = false;
				collisionData.collisionCheck = true;
				RigidbodyHandle handle;
				handle.handle = obj1RigidbodyHandle;
				collisionData.rigidbodyHandle = handle;
				collisions->second.insert({obj1Handle, collisionData});
			}

			return 0;
		}
	};

	PhysicsEngine::PhysicsEngine()
	{
		m_rigidBodyColliderDatas.resize(PhysicsEngine::RESIZE_RIGIDBODY_SIZE);

		m_collisionShapes.resize(PhysicsEngine::RESIZE_COLLISIONSHAPE_SIZE);

		m_collisionCallback = std::make_unique<BulletCollisionCallback>();
	}

	void PhysicsEngine::AddMeshColliderWaitForModel(const MeshWaitData& meshColliderData)
	{
		s_physicsEngine.m_meshCollidersWaitingForModels.push_back(meshColliderData);
	}

	btDiscreteDynamicsWorld* PhysicsEngine::GetDynamicsWorld()
	{
		return s_physicsEngine.m_dynamicsWorld.get();
	}

	PhysicsEngine::~PhysicsEngine()
	{
		//Delete rigidbodys
		for (u32 i = 0; i < m_rigidBodyColliderDatas.size(); ++i)
		{
			btRigidBody* body = m_rigidBodyColliderDatas[i].rigidBody;
			if (body == nullptr)
				continue;

			if (body->getMotionState())
			{
				delete m_rigidBodyColliderDatas[i].motionState;
				m_rigidBodyColliderDatas[i].motionState = nullptr;
			}
			m_dynamicsWorld->removeRigidBody(body);
			delete m_rigidBodyColliderDatas[i].rigidBody;
			m_rigidBodyColliderDatas[i].rigidBody = nullptr;
		}

		//Delete collisionShapes
		for (u32 i = 0; i < m_collisionShapes.size(); ++i)
		{
			if (m_collisionShapes[i])
			{
				delete m_collisionShapes[i];
				m_collisionShapes[i] = nullptr;
			}
		}

		m_rigidBodyColliderDatas.clear();

		//m_dynamicsWorld.release();
		//m_sequentialImpulseContraintSolver.release();
		//m_broadphaseInterface.release();
		//m_collisionDispatcher.release();
		//m_collisionConfiguration.release();
	}

	void PhysicsEngine::Initialize()
	{
		s_physicsEngine.m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
		s_physicsEngine.m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(s_physicsEngine.m_collisionConfiguration.get());
		s_physicsEngine.m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
		s_physicsEngine.m_sequentialImpulseContraintSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
		s_physicsEngine.m_dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(s_physicsEngine.m_collisionDispatcher.get(),
			s_physicsEngine.m_broadphaseInterface.get(), s_physicsEngine.m_sequentialImpulseContraintSolver.get(), s_physicsEngine.m_collisionConfiguration.get());

		s_physicsEngine.m_dynamicsWorld->setGravity({0.0f, -9.82f, 0.0f});
	}

	void PhysicsEngine::UpdatePhysics(float deltaTime)
	{
		s_physicsEngine.CheckMeshColliders();

		s_physicsEngine.GetDynamicsWorld()->stepSimulation(deltaTime, 10);

		EntityManager::Get().Collect<TransformComponent, BoxColliderComponent>().Do([&](TransformComponent& transform, BoxColliderComponent& collider)
			{
				//Get rigidbody
				auto& rigidBody = s_physicsEngine.m_rigidBodyColliderDatas[collider.rigidbodyHandle.handle];
				if (rigidBody.dynamic && rigidBody.rigidBody->getMotionState())
				{
					btTransform trans;
					rigidBody.rigidBody->getMotionState()->getWorldTransform(trans);
					trans.getOpenGLMatrix((float*)(&transform.worldMatrix));
				}
			});
		
		EntityManager::Get().Collect<TransformComponent, SphereColliderComponent>().Do([&](TransformComponent& transform, SphereColliderComponent& collider)
			{
				//Get rigidbody
				auto& rigidBody = s_physicsEngine.m_rigidBodyColliderDatas[collider.rigidbodyHandle.handle];
				if (rigidBody.dynamic && rigidBody.rigidBody->getMotionState())
				{
					btTransform trans;
					rigidBody.rigidBody->getMotionState()->getWorldTransform(trans);
					trans.getOpenGLMatrix((float*)(&transform.worldMatrix));
				}
			});

		EntityManager::Get().Collect<TransformComponent, CapsuleColliderComponent>().Do([&](TransformComponent& transform, CapsuleColliderComponent& collider)
			{
				//Get rigidbody
				auto& rigidBody = s_physicsEngine.m_rigidBodyColliderDatas[collider.rigidbodyHandle.handle];
				if (rigidBody.dynamic && rigidBody.rigidBody->getMotionState())
				{
					btTransform trans;
					rigidBody.rigidBody->getMotionState()->getWorldTransform(trans);
					trans.getOpenGLMatrix((float*)(&transform.worldMatrix));
				}
			});

		EntityManager::Get().Collect<RigidbodyComponent>().Do([&](RigidbodyComponent& rigidbody)
			{
				//Get rigidbody
				auto& rigidBody = s_physicsEngine.m_rigidBodyColliderDatas[rigidbody.rigidbodyHandle.handle];
				if (rigidBody.rigidBody && rigidBody.rigidBody->getMotionState())
				{
					//Check collisions
					PhysicsEngine::s_physicsEngine.GetDynamicsWorld()->contactTest(rigidBody.rigidBody, *(PhysicsEngine::s_physicsEngine.m_collisionCallback.get()));

					//Get handle for vector
					u32 handle = PhysicsEngine::s_physicsEngine.m_handleAllocator.GetSlot(rigidbody.rigidbodyHandle.handle);
					auto collisions = s_physicsEngine.m_rigidbodyCollision.find(handle);

					//Check if the existing collisions are exiting or have just entered the collision
					for (auto it = collisions->second.begin(); it != collisions->second.end();)
					{
						if (collisions->second.size() == 0)
							break;


						bool incrementIterator = true;

						//Check if the activeCollision has changed, if it has then we either call onCollisionEnter or onCollisionExit
						bool beforeCollision = it->second.activeCollision;
						it->second.activeCollision = it->second.collisionCheck;
						if (it->second.activeCollision != beforeCollision)
						{
							//Get RigidbodyColliderData to get the corresponding entities
							RigidbodyColliderData* obj0RigidbodyColliderData = PhysicsEngine::GetRigidbodyColliderData(rigidbody.rigidbodyHandle);
							RigidbodyColliderData* obj1RigidbodyColliderData = PhysicsEngine::GetRigidbodyColliderData(it->second.rigidbodyHandle);

							if (it->second.activeCollision && rigidbody.onCollisionEnter != nullptr)
								rigidbody.onCollisionEnter(obj0RigidbodyColliderData->rigidbodyEntity, obj1RigidbodyColliderData->rigidbodyEntity);
							else if (it->second.activeCollision)
								LuaMain::GetScriptManager()->CallFunctionOnAllEntityScripts(obj0RigidbodyColliderData->rigidbodyEntity, "OnCollisionEnter", obj1RigidbodyColliderData->rigidbodyEntity);

							//Set collisionCheck false for next collision check
							it->second.collisionCheck = false;

							if (!it->second.activeCollision)
							{
								if (rigidbody.onCollisionExit != nullptr)
									rigidbody.onCollisionExit(obj0RigidbodyColliderData->rigidbodyEntity, obj1RigidbodyColliderData->rigidbodyEntity);
								else
									LuaMain::GetScriptManager()->CallFunctionOnAllEntityScripts(obj0RigidbodyColliderData->rigidbodyEntity, "OnCollisionExit", obj1RigidbodyColliderData->rigidbodyEntity);

								//Remove the collision because we do not need to keep track of it anymore
 								collisions->second.erase(it++);
								incrementIterator = false;
							}
						}
						else
						{
							//Set collisionCheck false for next collision check
							it->second.collisionCheck = false;
						}

						if (incrementIterator)
							++it;
					}
				}
			});
	}

	RigidbodyHandle PhysicsEngine::AddRigidbodyColliderData(RigidbodyColliderData rigidbodyColliderData)
	{
		RigidbodyHandle rigidbodyHandle = s_physicsEngine.m_handleAllocator.Allocate<RigidbodyHandle>();
		u32 handle = gfx::HandleAllocator::GetSlot(rigidbodyHandle.handle);

		//Resize if needed
		if (handle >= s_physicsEngine.m_rigidBodyColliderDatas.size())
			s_physicsEngine.m_rigidBodyColliderDatas.resize(s_physicsEngine.m_rigidBodyColliderDatas.size() + PhysicsEngine::RESIZE_RIGIDBODY_SIZE);

		s_physicsEngine.m_rigidBodyColliderDatas[handle] = std::move(rigidbodyColliderData);

		return rigidbodyHandle;
	}

	RigidbodyHandle PhysicsEngine::AddRigidbody(entity entity, RigidbodyColliderData& rigidbodyColliderData, bool dynamic, float mass)
	{
		//Set rigidody entity
		rigidbodyColliderData.rigidbodyEntity = entity;

		TransformComponent& transform = EntityManager::Get().GetComponent<TransformComponent>(entity);

		//Copy entity transform
		btTransform groundTransform;
		groundTransform.setFromOpenGLMatrix((float*)(&transform.worldMatrix));

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = dynamic;
		float bodyMass = 0.0f;

		btCollisionShape* collisionShape = s_physicsEngine.GetCollisionShape(rigidbodyColliderData.collisionShapeHandle);
		btVector3 localInertia(0, 0, 0);
		if (isDynamic)
		{
			collisionShape->calculateLocalInertia(mass, localInertia);
			bodyMass = mass;
		}

		//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
		rigidbodyColliderData.motionState = new btDefaultMotionState(groundTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(bodyMass, rigidbodyColliderData.motionState, collisionShape, localInertia);
		rigidbodyColliderData.rigidBody = new btRigidBody(rbInfo);

		//Keep track if the rigidbody is dynamic or not
		rigidbodyColliderData.dynamic = dynamic;

		//add the body to the dynamics world
		PhysicsEngine::GetDynamicsWorld()->addRigidBody(rigidbodyColliderData.rigidBody);

		//Get the handle
		RigidbodyHandle rigidbodyHandle = PhysicsEngine::AddRigidbodyColliderData(std::move(rigidbodyColliderData));

		RigidbodyColliderData* pointerToRCD= PhysicsEngine::GetRigidbodyColliderData(rigidbodyHandle);

		//Sets the handle for collision detection later
		const u64 bitMask = 0x0000FFFF;
		const u32 byteShift = 4;
		u32 userIndex1 = (u32)(rigidbodyHandle.handle & bitMask);
		u32 userIndex2 = (u32)((rigidbodyHandle.handle >> byteShift) & bitMask);
		pointerToRCD->rigidBody->setUserIndex(userIndex1);
		pointerToRCD->rigidBody->setUserIndex2(userIndex2);

		return rigidbodyHandle;
	}

	RigidbodyColliderData* PhysicsEngine::GetRigidbodyColliderData(const RigidbodyHandle& rigidbodyHandle)
	{
		//Get handle for vector
		u32 handle = PhysicsEngine::s_physicsEngine.m_handleAllocator.GetSlot(rigidbodyHandle.handle);

		return &s_physicsEngine.m_rigidBodyColliderDatas[handle];
	}

	void PhysicsEngine::CheckMeshColliders()
	{
		//Check mesh colliders until the model is loaded into memory
		for (u32 index = 0; index < m_meshCollidersWaitingForModels.size(); ++index)
		{
			ModelAsset* model = AssetManager::Get().GetAsset<ModelAsset>(m_meshCollidersWaitingForModels[index].meshModelID);
			if (model)
			{
				MeshColliderComponent component = EntityManager::Get().GetComponent<MeshColliderComponent>(m_meshCollidersWaitingForModels[index].meshEntity);
				component.LoadMesh(m_meshCollidersWaitingForModels[index].meshEntity, m_meshCollidersWaitingForModels[index].meshModelID);
				m_meshCollidersWaitingForModels.erase(m_meshCollidersWaitingForModels.begin() + index);
				--index;
			}
		}
	}

	void PhysicsEngine::AddMeshColliderData(const MeshColliderData& meshColliderData)
	{
		s_physicsEngine.m_meshCollidersLoadedInMemory.push_back(meshColliderData);
	}

	MeshColliderData PhysicsEngine::GetMeshColliderData(u32 modelID)
	{
		//Get mesh collider data if it exists
		for (auto& data : s_physicsEngine.m_meshCollidersLoadedInMemory)
		{
			if (data.meshModelID == modelID)
			{
				return data;
			}
		}

		MeshColliderData meshColliderData;
		meshColliderData.collisionShapeHandle.handle = 0;
		meshColliderData.meshModelID = 0;
		return meshColliderData;
	}

	CollisionShapeHandle PhysicsEngine::AddCollisionShape(btCollisionShape* addCollisionShape)
	{
		CollisionShapeHandle collisionShapeHandle = s_physicsEngine.m_handleAllocator.Allocate<CollisionShapeHandle>();
		u32 handle = gfx::HandleAllocator::GetSlot(collisionShapeHandle.handle);

		//Resize if needed
		if (handle >= s_physicsEngine.m_collisionShapes.size())
			s_physicsEngine.m_collisionShapes.resize(s_physicsEngine.m_collisionShapes.size() + PhysicsEngine::RESIZE_COLLISIONSHAPE_SIZE);

		s_physicsEngine.m_collisionShapes[handle] = addCollisionShape;

		return collisionShapeHandle;
	}

	btCollisionShape* PhysicsEngine::GetCollisionShape(const CollisionShapeHandle& collisionShapeHandle)
	{
		u32 handle = gfx::HandleAllocator::GetSlot(collisionShapeHandle.handle);

		//0 is default for HandleAllocator
		if (handle == 0)
		{
			std::cout << "Handle does not exist!\n";
			assert(false);
		}

		return m_collisionShapes[handle];
	}

	BoxColliderComponent::BoxColliderComponent(entity entity, const DirectX::SimpleMath::Vector3& boxColliderSize, bool dynamic, float mass) noexcept
	{
		RigidbodyColliderData rCD; 
		rCD.collisionShapeHandle = PhysicsEngine::AddCollisionShape(new btBoxShape(btVector3(boxColliderSize.x, boxColliderSize.y, boxColliderSize.z)));

		rigidbodyHandle = PhysicsEngine::AddRigidbody(entity, rCD, dynamic, mass);
	}

	SphereColliderComponent::SphereColliderComponent(entity entity, float radius, bool dynamic, float mass) noexcept
	{
		RigidbodyColliderData rCD;
		rCD.collisionShapeHandle = PhysicsEngine::AddCollisionShape(new btSphereShape(radius));

		rigidbodyHandle = PhysicsEngine::AddRigidbody(entity, rCD, dynamic, mass);
	}

	CapsuleColliderComponent::CapsuleColliderComponent(entity entity, float radius, float height, bool dynamic, float mass) noexcept
	{
		RigidbodyColliderData rCD;
		rCD.collisionShapeHandle = PhysicsEngine::AddCollisionShape(new btCapsuleShape(radius, height));

		rigidbodyHandle = PhysicsEngine::AddRigidbody(entity, rCD, dynamic, mass);
	}

	RigidbodyComponent::RigidbodyComponent(entity entity)
	{
		//Can only create a rigidbody component for box, sphere, capsule
		if (EntityManager::Get().HasComponent<BoxColliderComponent>(entity))
		{
			rigidbodyHandle = EntityManager::Get().GetComponent<BoxColliderComponent>(entity).rigidbodyHandle;
		}
		else if (EntityManager::Get().HasComponent<SphereColliderComponent>(entity))
		{
			rigidbodyHandle = EntityManager::Get().GetComponent<SphereColliderComponent>(entity).rigidbodyHandle;
		}
		else if (EntityManager::Get().HasComponent<CapsuleColliderComponent>(entity))
		{
			rigidbodyHandle = EntityManager::Get().GetComponent<CapsuleColliderComponent>(entity).rigidbodyHandle;
		}
		else
		{
			std::cout << "Entity has no collider component for rigidbody\n";
			assert(false);
		}

		//Get handle for vector
		u32 handle = PhysicsEngine::s_physicsEngine.m_handleAllocator.GetSlot(rigidbodyHandle.handle);

		//Set up rigidbody for collision
		PhysicsEngine::s_physicsEngine.m_rigidbodyCollision.insert({ handle, {} });
	}

	void RigidbodyComponent::SetOnCollisionEnter(std::function<void(entity, entity)> inOnCollisionEnter)
	{
		onCollisionEnter = inOnCollisionEnter;
	}

	void RigidbodyComponent::SetOnCollisionExit(std::function<void(entity, entity)> inOnCollisionExit)
	{
		onCollisionExit = inOnCollisionExit;
	}

	void RigidbodyComponent::ConstrainRotation(bool constrainXRotation, bool constrainYRotation, bool constrainZRotation)
	{
		RigidbodyColliderData* rigidbodyColliderData = PhysicsEngine::GetRigidbodyColliderData(rigidbodyHandle);

		//Set no rotations in x,y,z
		float x = constrainXRotation ? 0.0f : 1.0f;
		float y = constrainYRotation ? 0.0f : 1.0f;
		float z = constrainZRotation ? 0.0f : 1.0f;

		rigidbodyColliderData->rigidBody->setAngularFactor(btVector3(x, y, z));
	}

	void RigidbodyComponent::ConstrainPosition(bool constrainXPosition, bool constrainYPosition, bool constrainZPosition)
	{
		RigidbodyColliderData* rigidbodyColliderData = PhysicsEngine::GetRigidbodyColliderData(rigidbodyHandle);

		////Set freeze position in x,y,z
		float x = constrainXPosition ? 0.0f : 1.0f;
		float y = constrainYPosition ? 0.0f : 1.0f;
		float z = constrainZPosition ? 0.0f : 1.0f;

		rigidbodyColliderData->rigidBody->setLinearFactor(btVector3(x, y, z));
	}

	MeshColliderComponent::MeshColliderComponent(entity entity, u32 modelID) noexcept
	{	
		AssetFlags modelFlags = AssetManager::Get().GetAssetFlags(modelID);

		bool modelLoadingToCPU = modelFlags.loadFlag & AssetLoadFlag::CPUMemory;
		bool modelOnCPU = modelFlags.stateFlag & AssetStateFlag::ExistOnCPU;

		//If the model is neither being loaded to the cpu memory or does not exist in cpu memory, we assert!
		if (!(modelLoadingToCPU || modelOnCPU))
		{
			std::cout << "Asset does not have CPUMemory flag set!\nMeshColliderComponent require the mesh to be on the cpu!\n";
			assert(false);
			return;
		}

		ModelAsset* model = AssetManager::Get().GetAsset<ModelAsset>(modelID);

		//Check if the model is loaded into memory
		if (!model)
		{
			MeshWaitData meshWaitData;
			meshWaitData.meshEntity = entity;
			meshWaitData.meshModelID = modelID;
			PhysicsEngine::AddMeshColliderWaitForModel(meshWaitData);
			return;
		}

		//The model is loaded in
		LoadMesh(entity, modelID);
	}

	void MeshColliderComponent::LoadMesh(entity entity, u32 modelID)
	{
		RigidbodyColliderData rCD;

		//Get mesh collider for an already existing mesh collider if it exists
		MeshColliderData meshColliderData = PhysicsEngine::GetMeshColliderData(modelID);

		//Set the handle, if it is zero we create a new collisionShape
		rCD.collisionShapeHandle = meshColliderData.collisionShapeHandle;

		//For the handle 0 is default value
		if (meshColliderData.collisionShapeHandle.handle == 0)
		{
			btTriangleMesh* mesh = new btTriangleMesh();
			ModelAsset* model = AssetManager::Get().GetAsset<ModelAsset>(modelID);

			if (!model)
			{
				//Should never happen!!!
				assert(false);
			}

			struct Vertex
			{
				float x;
				float y;
				float z;
			};
			const u32 verticePerTriangle = 3;

			std::vector<u8>* vertexData = &(model->meshAsset.vertexData[VertexAttribute::Position]);
			Vertex* vertexVertices = (Vertex*)vertexData->data();

			u32 trianglesAmount = (u32)(model->meshAsset.indices.size() / verticePerTriangle);
			u32 verticesAmount = (u32)(vertexData->size() / (sizeof(Vertex)));

			//Set the mesh for the collider
			btIndexedMesh indexedMesh;
			indexedMesh.m_numTriangles = trianglesAmount;
			indexedMesh.m_triangleIndexBase = (const unsigned char*)model->meshAsset.indices.data();
			indexedMesh.m_triangleIndexStride = verticePerTriangle * sizeof(u32);
			indexedMesh.m_numVertices = verticesAmount;
			indexedMesh.m_vertexBase = (const unsigned char*)vertexVertices;
			indexedMesh.m_vertexStride = sizeof(Vertex);

			mesh->addIndexedMesh(indexedMesh);

			btCollisionShape* meshCollider = new btBvhTriangleMeshShape(mesh, true);

			//Add the mesh to the existing mesh vector
			MeshColliderData newMeshColliderData;
			newMeshColliderData.meshModelID = modelID;
			newMeshColliderData.collisionShapeHandle = PhysicsEngine::AddCollisionShape(meshCollider);
			PhysicsEngine::AddMeshColliderData(newMeshColliderData);

			rCD.collisionShapeHandle = newMeshColliderData.collisionShapeHandle;
		}

		//Meshes can not be dynamic
		//Convex meshes can be
		float mass = 0.0f;
		bool dynamic = false;
		rigidbodyHandle = PhysicsEngine::AddRigidbody(entity, rCD, dynamic, mass);
		meshNotLoaded = false;
	}
}