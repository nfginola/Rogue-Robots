#pragma once
#include <DOGEngine.h>
#include "Scene.h"

class LightScene : public Scene
{
public:
	LightScene();
	~LightScene();
	void SetUpScene(std::vector<std::function<std::vector<DOG::entity>()>> entityCreators = {}) override;
	void Update();
private:
	DOG::entity AddFrustum(DirectX::SimpleMath::Matrix projetion, DirectX::SimpleMath::Matrix view);
	DOG::entity AddFrustum(DirectX::SimpleMath::Vector4 leftPlane, DirectX::SimpleMath::Vector4 rightPlane,
		DirectX::SimpleMath::Vector4 bottomPlane, DirectX::SimpleMath::Vector4 topPlane,
		DirectX::SimpleMath::Vector4 nearPlane, DirectX::SimpleMath::Vector4 farPlane);
	//DOG::entity AddFrustumDXTK(DirectX::SimpleMath::Matrix projetion, DirectX::SimpleMath::Matrix view);
	DOG::entity AddFace(const std::vector<DirectX::SimpleMath::Vector3>& vertexPoints, DirectX::SimpleMath::Vector3 color);
	DOG::entity AddSphere(DirectX::SimpleMath::Vector3 center, float radius, DirectX::SimpleMath::Vector3 color);
	
	void TiledShadingDebugMenu(bool& open);

	void LightCullingDebugMenu(bool& open);

	bool m_testWindowOpen = false;

};