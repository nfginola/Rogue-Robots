#include "FrontRenderer.h"
#include "Renderer.h"
#include "../../Core/AssetManager.h"			// Asset translation
#include "../../Physics/PhysicsEngine.h"		// Collider components

// Exposed graphics managers
#include "../../Core/CustomMaterialManager.h"
#include "../../Core/CustomMeshManager.h"
#include "../../Core/LightManager.h"		

namespace DOG::gfx
{
	FrontRenderer::FrontRenderer(Renderer* renderer) :
		m_renderer(renderer)
	{
		LightManager::Initialize(m_renderer);
		CustomMeshManager::Initialize(m_renderer);
		CustomMaterialManager::Initialize(m_renderer);

	}

	FrontRenderer::~FrontRenderer()
	{
		LightManager::Destroy();
		CustomMeshManager::Destroy();
		CustomMaterialManager::Destroy();
	}

	void FrontRenderer::BeginFrameUICapture()
	{
		m_renderer->BeginGUI();
	}

	void FrontRenderer::BeginGPUFrame()
	{
		m_renderer->BeginFrame_GPU();
	}

	void FrontRenderer::EndGPUFrame()
	{
		m_renderer->EndFrame_GPU(true);
	}

	void FrontRenderer::Update(f32)
	{
		UpdateLights();
		GatherShadowCasters();
		SetRenderCamera();
		GatherDrawCalls();

		// Update internal data structures
		m_renderer->Update(0.f);
	}

	void FrontRenderer::UpdateLights()
	{
		// Update lights
		EntityManager::Get().Collect<DirtyComponent, PointLightComponent>().Do([](entity, DirtyComponent& dirty, PointLightComponent& light) {
			light.dirty |= dirty.IsDirty(DirtyComponent::positionChanged); });

		EntityManager::Get().Collect<DirtyComponent, SpotLightComponent>().Do([](entity, DirtyComponent& dirty, SpotLightComponent& light) {
			light.dirty |= dirty.IsDirty(DirtyComponent::positionChanged) || dirty.IsDirty(DirtyComponent::rotationChanged); });

		EntityManager::Get().Collect<TransformComponent, SpotLightComponent>().Do([&](entity, TransformComponent tr, SpotLightComponent& light)
			{
				if (light.dirty)
				{
					SpotLightDesc d{};
					d.position = tr.GetPosition();
					d.color = light.color;
					d.cutoffAngle = light.cutoffAngle;
					d.direction = light.direction;
					d.strength = light.strength;
					d.id = light.id;
					LightManager::Get().UpdateSpotLight(light.handle, d);
					light.dirty = false;
				}
			});

		EntityManager::Get().Collect<TransformComponent, PointLightComponent>().Do([&](entity, TransformComponent tr, PointLightComponent& light)
			{
				if (light.dirty)
				{
					PointLightDesc d{};
					d.position = tr.GetPosition();
					d.color = light.color;
					d.strength = light.strength;
					LightManager::Get().UpdatePointLight(light.handle, d);
					light.dirty = false;
				}
			});
	}

	void FrontRenderer::GatherDrawCalls()
	{
		EntityManager::Get().Collect<TransformComponent, SubmeshRenderer>().Do([&](entity e, TransformComponent& tr, SubmeshRenderer& sr)
			{
				// We are assuming that this is a totally normal submesh with no weird branches (i.e on ModularBlock or whatever)
				if (EntityManager::Get().HasComponent<ShadowReceiverComponent>(e))
				{
					m_renderer->SubmitShadowMesh(sr.mesh, 0, sr.material, tr);
				}
				if (sr.dirty)
					CustomMaterialManager::Get().UpdateMaterial(sr.material, sr.materialDesc);
				m_renderer->SubmitMesh(sr.mesh, 0, sr.material, tr);
			});


		// We need to bucket in a better way..
		EntityManager::Get().Collect<TransformComponent, ModelComponent>().Do([&](entity e, TransformComponent& transformC, ModelComponent& modelC)
			{
				MINIPROFILE_NAMED("RenderSystem")
					ModelAsset* model = AssetManager::Get().GetAsset<ModelAsset>(modelC);
				if (model && model->gfxModel)
				{
					// Shadow submission:
					if (EntityManager::Get().HasComponent<ShadowReceiverComponent>(e))
					{
						for (u32 i = 0; i < model->gfxModel->mesh.numSubmeshes; ++i)
						{
							if (EntityManager::Get().HasComponent<ModularBlockComponent>(e))
								m_renderer->SubmitShadowMeshNoFaceCulling(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
							else
								m_renderer->SubmitShadowMesh(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
						}
					}


					if (EntityManager::Get().HasComponent<ModularBlockComponent>(e))
					{
						if (EntityManager::Get().HasComponent<MeshColliderComponent>(e) &&
							EntityManager::Get().GetComponent<MeshColliderComponent>(e).drawMeshColliderOverride)
						{
							u32 meshColliderModelID = EntityManager::Get().GetComponent<MeshColliderComponent>(e).meshColliderModelID;
							ModelAsset* meshColliderModel = AssetManager::Get().GetAsset<ModelAsset>(meshColliderModelID);
							if (meshColliderModel && meshColliderModel->gfxModel)
							{
								for (u32 i = 0; i < meshColliderModel->gfxModel->mesh.numSubmeshes; ++i)
									m_renderer->SubmitMeshWireframeNoFaceCulling(meshColliderModel->gfxModel->mesh.mesh, i, meshColliderModel->gfxModel->mats[i], transformC);
							}
						}
						else
						{
							for (u32 i = 0; i < model->gfxModel->mesh.numSubmeshes; ++i)
								m_renderer->SubmitMeshNoFaceCulling(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
						}
					}
					else if (EntityManager::Get().HasComponent<AnimationComponent>(e))
					{
						for (u32 i = 0; i < model->gfxModel->mesh.numSubmeshes; ++i)
							m_renderer->SubmitAnimatedMesh(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
					}
					else
					{
						if (EntityManager::Get().HasComponent<MeshColliderComponent>(e) &&
							EntityManager::Get().GetComponent<MeshColliderComponent>(e).drawMeshColliderOverride)
						{
							for (u32 i = 0; i < model->gfxModel->mesh.numSubmeshes; ++i)
								m_renderer->SubmitMeshWireframe(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
						}
						else
						{
							for (u32 i = 0; i < model->gfxModel->mesh.numSubmeshes; ++i)
								m_renderer->SubmitMesh(model->gfxModel->mesh.mesh, i, model->gfxModel->mats[i], transformC);
						}
					}
				}
			});
	}

	void FrontRenderer::SetRenderCamera()
	{
		CameraComponent cameraComponent;
		EntityManager::Get().Collect<CameraComponent>().Do([&](CameraComponent& c)
			{
				if (c.isMainCamera)
				{
					cameraComponent = c;
				}
			});

		auto& proj = (DirectX::XMMATRIX&)cameraComponent.projMatrix;
		m_renderer->SetMainRenderCamera(cameraComponent.viewMatrix, &proj);

	}

	void FrontRenderer::GatherShadowCasters()
	{
		// Collect this frames spotlight shadow casters
		m_activeSpotlightShadowCasters.clear();
		EntityManager::Get().Collect<ShadowCasterComponent, SpotLightComponent, CameraComponent, TransformComponent>().Do([&](
			entity spotlightEntity, ShadowCasterComponent& sc, SpotLightComponent& slc, CameraComponent& cc, TransformComponent& tc)
			{
				m_activeSpotlightShadowCasters.insert(spotlightEntity);
				
				// Register this frames spotlights
				Renderer::ActiveSpotlight spotData{};
				spotData.shadow = Renderer::ShadowCaster();

				spotData.shadow->viewMat = cc.viewMatrix;
				spotData.shadow->projMat = cc.projMatrix;
				spotData.position = { tc.GetPosition().x, tc.GetPosition().y, tc.GetPosition().z, 1.0f };
				spotData.color = { slc.color.x, slc.color.y, slc.color.z, };
				spotData.direction = slc.direction;
				spotData.cutoffAngle = slc.cutoffAngle;
				spotData.strength = slc.strength;
				
				m_renderer->RegisterSpotlight(spotData);
			});

		// Update renderer shadow map capacity
		if (m_activeSpotlightShadowCasters.size() > m_shadowMapCapacity)
		{
			std::cout << "dirty! went from " << m_shadowMapCapacity << " to " << m_activeSpotlightShadowCasters.size() << "\n";
			m_shadowMapCapacity = (u32)m_activeSpotlightShadowCasters.size();
			// set renderer to dirty
		}
	}

	void FrontRenderer::Render(f32)
	{
		Update(0.f);

		m_renderer->Render(0.f);
	}

	void FrontRenderer::PerformDeferredDeletion()
	{
		LightManager::Get().DestroyDeferredEntities();

	}


}