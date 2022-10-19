#include "Scene.h"
#include "../ECS/EntityManager.h"

namespace DOG
{
	EntityManager& Scene::s_entityManager = EntityManager::Get();

	Scene::Scene(SceneType scene) : m_sceneType(scene) {}

	Scene::~Scene()
	{
		s_entityManager.Collect<SceneComponent>().Do([&](entity e, SceneComponent& sceneC)
			{
				if (sceneC.scene == m_sceneType)
				{
					s_entityManager.DestroyEntity(e);
				}
			});
	}

	entity Scene::CreateEntity() const noexcept
	{
		entity e = s_entityManager.CreateEntity();
		s_entityManager.AddComponent<SceneComponent>(e, m_sceneType);
		return e;
	}

	SceneType Scene::GetSceneType() const noexcept
	{
		return m_sceneType;
	}
}