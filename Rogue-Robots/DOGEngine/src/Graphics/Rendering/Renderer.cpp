#include "Renderer.h"

#include "../RHI/DX12/RenderBackend_DX12.h"
#include "../RHI/DX12/ImGUIBackend_DX12.h"
#include "../RHI/DX12/RenderDevice_DX12.h"
#include "../RHI/DX12/Swapchain_DX12.h"
#include "../RHI/ShaderCompilerDXC.h"
#include "../RHI/PipelineBuilder.h"

#include "GPUGarbageBin.h"
#include "UploadContext.h"
#include "GPUTable.h"
#include "GPUDynamicConstants.h"
#include "MaterialTable.h"
#include "MeshTable.h"
#include "LightTable.h"
#include "GraphicsBuilder.h"

#include "../../Core/AssimpImporter.h"
#include "../../Core/TextureFileImporter.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGResourceManager.h"
#include "RenderGraph/RGBlackboard.h"

#include "Tracy/Tracy.hpp"

// Passes
#include "RenderEffects/ImGUIEffect.h"
#include "RenderEffects/TestComputeEffect.h"
#include "RenderEffects/Bloom.h"
#include "VFX/ParticleBackend.h"
#include "RenderEffects/TiledLightCullingEffect.h"
#include "RenderEffects/DamageDiskEffect.h"

#include "ImGUI/imgui.h"
#include "../../Core/ImGuiMenuLayer.h"
#include "../../common/MiniProfiler.h"

#include "PostProcess.h"

namespace DOG::gfx
{
	Renderer::Renderer(HWND hwnd, u32 clientWidth, u32 clientHeight, bool debug) :
		m_renderWidth(1920),
		m_renderHeight(1080)
	{
		m_jointMan = std::make_unique<AnimationManager>();
		m_backend = std::make_unique<gfx::RenderBackend_DX12>(debug);
		m_rd = m_backend->CreateDevice(S_NUM_BACKBUFFERS);
		m_sc = m_rd->CreateSwapchain(hwnd, (u8)S_NUM_BACKBUFFERS);
		UI::Initialize(m_rd, m_sc, S_NUM_BACKBUFFERS, clientWidth, clientHeight);
		PostProcess::Initialize();

		m_frameSyncs.resize(S_MAX_FIF);

		m_singleSidedShadowDraws.resize(12);
		m_doubleSidedShadowDraws.resize(12);

		AddScenes();
		UIRebuild(clientHeight, clientWidth);	

		m_imgui = std::make_unique<gfx::ImGUIBackend_DX12>(m_rd, m_sc, S_MAX_FIF);

		m_sclr = std::make_unique<ShaderCompilerDXC>();

		m_wmCallback = [this](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			return WinProc(hwnd, uMsg, wParam, lParam);
		};

		const u32 maxUploadSizeDefault = 40'000'000;
		const u32 maxUploadSizeTextures = 400'000'000;

		m_bin = std::make_unique<GPUGarbageBin>(S_MAX_FIF);
		m_uploadCtx = std::make_unique<UploadContext>(m_rd, maxUploadSizeDefault, S_MAX_FIF);
		m_texUploadCtx = std::make_unique<UploadContext>(m_rd, maxUploadSizeTextures, S_MAX_FIF);
		m_meshUploadCtx = std::make_unique<UploadContext>(m_rd, maxUploadSizeDefault, S_MAX_FIF);

		// For internal per frame management
		const u32 maxUploadPerFrame = 512'000;
		m_perFrameUploadCtx = std::make_unique<UploadContext>(m_rd, maxUploadPerFrame, S_MAX_FIF, QueueType::Copy);



		const u32 maxConstantsPerFrame = 150'000;
		m_dynConstants = std::make_unique<GPUDynamicConstants>(m_rd, m_bin.get(), maxConstantsPerFrame);
		m_dynConstantsTemp = std::make_unique<GPUDynamicConstants>(m_rd, m_bin.get(), 3 * 4 * 24);


		// multiple of curr loaded mixamo skeleton
		m_dynConstantsAnimated = std::make_unique<GPUDynamicConstants>(m_rd, m_bin.get(), 75 * 100 * S_MAX_FIF);
		m_dynConstantsAnimatedShadows = std::make_unique<GPUDynamicConstants>(m_rd, m_bin.get(), 75 * 100 * m_shadowMapCapacity * S_MAX_FIF);
		m_cmdl = m_rd->AllocateCommandList();

		// Startup
		MeshTable::MemorySpecification spec{};
		const u32 maxBytesPerAttribute = 4'000'000;
		const u32 maxNumberOfIndices = 1'000'000;
		const u32 maxTotalSubmeshes = 500;
		const u32 maxMaterialArgs = 1000;

		spec.maxSizePerAttribute[VertexAttribute::Position] = maxBytesPerAttribute;
		spec.maxSizePerAttribute[VertexAttribute::UV] = maxBytesPerAttribute;
		spec.maxSizePerAttribute[VertexAttribute::Normal] = maxBytesPerAttribute;
		spec.maxSizePerAttribute[VertexAttribute::Tangent] = maxBytesPerAttribute;
		spec.maxSizePerAttribute[VertexAttribute::BlendData] = maxBytesPerAttribute;
		spec.maxTotalSubmeshes = maxTotalSubmeshes;
		spec.maxNumIndices = maxNumberOfIndices;
		m_globalMeshTable = std::make_unique<MeshTable>(m_rd, m_bin.get(), spec);

		MaterialTable::MemorySpecification memSpec{};
		memSpec.maxElements = maxMaterialArgs;
		m_globalMaterialTable = std::make_unique<MaterialTable>(m_rd, m_bin.get(), memSpec);

		// Default storage
		auto lightStorageSpec = LightTable::StorageSpecification();
		lightStorageSpec.pointLightSpec.maxStatics = 25;
		lightStorageSpec.pointLightSpec.maxDynamic = 512;
		m_globalLightTable = std::make_unique<LightTable>(m_rd, m_bin.get(), lightStorageSpec, false);



		// Create builder for users to create graphical objects supported by the renderer
		m_builder = std::make_unique<GraphicsBuilder>(
			m_rd,
			m_uploadCtx.get(),
			m_texUploadCtx.get(),
			m_globalMeshTable.get(),
			m_globalMaterialTable.get(),
			m_bin.get());


		// INITIALIZE RESOURCES ================= Globals for now..
		// Refer to TestComputeEffect for Effect local resource building
		auto fullscreenTriVS = m_sclr->CompileFromFile("FullscreenTriVS.hlsl", ShaderType::Vertex);
		auto blitPS = m_sclr->CompileFromFile("BlitPS.hlsl", ShaderType::Pixel);
		m_pipe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(fullscreenTriVS.get())
			.SetShader(blitPS.get())
			.AppendRTFormat(m_sc->GetBufferFormat())
			.Build());

		auto meshVS = m_sclr->CompileFromFile("MainVS.hlsl", ShaderType::Vertex);
		auto meshPS = m_sclr->CompileFromFile("MainPS.hlsl", ShaderType::Pixel);
		m_meshPipe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(meshVS.get())
			.SetShader(meshPS.get())
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true).SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO).SetDepthFunc(D3D12_COMPARISON_FUNC_EQUAL))
			.Build());

		auto weaponMeshVS = m_sclr->CompileFromFile("WeaponVS.hlsl", ShaderType::Vertex);
		m_weaponMeshPipe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(weaponMeshVS.get())
			.SetShader(meshPS.get())
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.Build());

		auto shadowVS = m_sclr->CompileFromFile("ShadowVS.hlsl", ShaderType::Vertex);
		//auto shadowGS = m_sclr->CompileFromFile("ShadowGS.hlsl", ShaderType::Geometry);
		auto shadowPS = m_sclr->CompileFromFile("ShadowPS.hlsl", ShaderType::Pixel);
		/*
			Remove GS usage
			reroute ShadowVS --> ShadowGS.hlsl if you want to enable it again (better to avoid)
		*/
		m_shadowPipe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(shadowVS.get())
			//.SetShader(shadowGS.get())			
			.SetShader(shadowPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.Build());

		m_shadowPipeNoCull = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(shadowVS.get())
			//.SetShader(shadowGS.get())
			.SetShader(shadowPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.SetRasterizer(RasterizerBuilder().SetCullMode(D3D12_CULL_MODE_NONE))
			.Build());

		m_meshPipeNoCull = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(meshVS.get())
			.SetShader(meshPS.get())
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)

			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true).SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO).SetDepthFunc(D3D12_COMPARISON_FUNC_EQUAL))
			.SetRasterizer(RasterizerBuilder().SetCullMode(D3D12_CULL_MODE_NONE))
			.Build());

		m_meshPipeWireframe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(meshVS.get())
			.SetShader(meshPS.get())
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)

			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true).SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO).SetDepthFunc(D3D12_COMPARISON_FUNC_EQUAL))
			.SetRasterizer(RasterizerBuilder().SetFillMode(D3D12_FILL_MODE_WIREFRAME))
			.Build());

		m_meshPipeWireframeNoCull = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(meshVS.get())
			.SetShader(meshPS.get())
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
			.AppendRTFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)

			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true).SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO).SetDepthFunc(D3D12_COMPARISON_FUNC_EQUAL))
			.SetRasterizer(RasterizerBuilder().SetFillMode(D3D12_FILL_MODE_WIREFRAME).SetCullMode(D3D12_CULL_MODE_NONE))
			.Build());


		auto zPrePassVS = m_sclr->CompileFromFile("ZPrePassVS.hlsl", ShaderType::Vertex);
		auto emptyPS = m_sclr->CompileFromFile("EmptyPS.hlsl", ShaderType::Pixel);
		m_zPrePassPipe = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(zPrePassVS.get())
			.SetShader(emptyPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.Build());

		m_zPrePassPipeNoCull = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(zPrePassVS.get())
			.SetShader(emptyPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.SetRasterizer(RasterizerBuilder().SetCullMode(D3D12_CULL_MODE_NONE))
			.Build());

		m_zPrePassPipeWirefram = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(zPrePassVS.get())
			.SetShader(emptyPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.SetRasterizer(RasterizerBuilder().SetFillMode(D3D12_FILL_MODE_WIREFRAME))
			.Build());

		m_zPrePassPipeWireframNoCull = m_rd->CreateGraphicsPipeline(GraphicsPipelineBuilder()
			.SetShader(zPrePassVS.get())
			.SetShader(emptyPS.get())
			.SetDepthFormat(DepthFormat::D32)
			.SetDepthStencil(DepthStencilBuilder().SetDepthEnabled(true))
			.SetRasterizer(RasterizerBuilder().SetFillMode(D3D12_FILL_MODE_WIREFRAME).SetCullMode(D3D12_CULL_MODE_NONE))
			.Build());

		auto ssaoCS = m_sclr->CompileFromFile("ssaoCS.hlsl", ShaderType::Compute);
		m_ssaoPipe = m_rd->CreateComputePipeline(ComputePipelineDesc(ssaoCS.get()));


		auto boxBlurCS = m_sclr->CompileFromFile("BoxBlurCS.hlsl", ShaderType::Compute);
		m_boxBlurPipe = m_rd->CreateComputePipeline(ComputePipelineDesc(boxBlurCS.get()));





		m_rgResMan = std::make_unique<RGResourceManager>(m_rd, m_bin.get());



		// Setup per frame
		m_pfDataTable = std::make_unique<GPUTableDeviceLocal<PfDataHandle>>(m_rd, m_bin.get(), (u32)sizeof(PerFrameData), S_MAX_FIF + 1, false);
		m_pfHandle = m_pfDataTable->Allocate(1, &m_pfData);


		// Setup persistent renderer data
		m_globalData.meshTableSubmeshMD = m_globalMeshTable->GetSubmeshDescriptor();
		m_globalData.meshTablePos = m_globalMeshTable->GetAttributeDescriptor(VertexAttribute::Position);
		m_globalData.meshTableUV = m_globalMeshTable->GetAttributeDescriptor(VertexAttribute::UV);
		m_globalData.meshTableNor = m_globalMeshTable->GetAttributeDescriptor(VertexAttribute::Normal);
		m_globalData.meshTableTan = m_globalMeshTable->GetAttributeDescriptor(VertexAttribute::Tangent);
		m_globalData.meshTableBlend = m_globalMeshTable->GetAttributeDescriptor(VertexAttribute::BlendData);
		m_globalData.perFrameTable = m_pfDataTable->GetGlobalDescriptor();
		m_globalData.materialTable = m_globalMaterialTable->GetDescriptor();
		m_globalData.pointLightTable = m_globalLightTable->GetDescriptor(LightType::Point);
		m_globalData.spotLightTable = m_globalLightTable->GetDescriptor(LightType::Spot);
		m_globalData.areaLightTable = m_globalLightTable->GetDescriptor(LightType::Area);
		m_globalData.lightTableMD = m_globalLightTable->GetMetadataDescriptor();

		m_globalDataTable = std::make_unique<GPUTableDeviceLocal<GlobalDataHandle>>(m_rd, m_bin.get(), (u32)sizeof(GlobalData), 1, false);
		m_gdHandle = m_globalDataTable->Allocate(1, &m_globalData);
		m_globalDataTable->SendCopyRequests(*m_perFrameUploadCtx);


		// Set default pass data
		m_globalEffectData.rd = m_rd;
		m_globalEffectData.sclr = m_sclr.get();
		m_globalEffectData.bbScissor = ScissorRects().Append(0, 0, clientWidth, clientHeight);
		m_globalEffectData.bbVP = Viewports().Append(0.f, 0.f, (f32)clientWidth, (f32)clientHeight);
		// render vps/scissors subject to change
		m_globalEffectData.defRenderScissors = ScissorRects().Append(0, 0, m_renderWidth, m_renderHeight);
		m_globalEffectData.defRenderVPs = Viewports().Append(0.f, 0.f, (f32)m_renderWidth, (f32)m_renderHeight);
		m_globalEffectData.globalDataDescriptor = m_globalDataTable->GetGlobalDescriptor();
		m_globalEffectData.meshTable = m_globalMeshTable.get();

		// Setup blackboard for potential Effect-intercom
		m_rgBlackboard = std::make_unique<RGBlackboard>();

		
		m_particleBackend = std::make_unique<ParticleBackend>(m_rd, m_bin.get(), S_MAX_FIF, m_globalEffectData, m_rgResMan.get());


		// Define Passes
		/*
			Remember that the connections between passes are simply resource names.

			You can see how the graph looks like by uncommenting the GENERATE_GRAPHVIZ define in RenderGraph.h.
			you can then find rendergraph.txt in the Assets folder and simply copy paste it to Graphviz Online.
			The graph is to be traversed in topological order, so you can then verify that everything is executed
			in the order you expect them to.

			Remember that you do not have to immediately create an Effect class to play around.
			You can simply define passes as lambdas just like Forward Pass in the Render function to get acquainted
			(i.e try defining a few passes with only read/write declarations to see if the generated graph is as expected!)

		*/
		m_imGUIEffect = std::make_unique<ImGUIEffect>(m_globalEffectData, m_imgui.get());
		m_testComputeEffect = std::make_unique<TestComputeEffect>(m_globalEffectData);
		m_bloomEffect = std::make_unique<Bloom>(m_rgResMan.get(), m_globalEffectData, m_dynConstants.get(), m_renderWidth, m_renderHeight);
		m_tiledLightCuller = std::make_unique<TiledLightCullingEffect>(m_rgResMan.get(), m_globalEffectData, m_renderWidth, m_renderHeight, 60.0f);
		m_tiledLightCullerVisualization = std::make_unique<TiledLightCullingVisualizationEffect>(m_rgResMan.get(), m_globalEffectData, m_renderWidth, m_renderHeight);

		m_damageDiskEffect = std::make_unique<DamageDiskEffect>(m_globalEffectData);
	
		{
			// Create 4x4 SSAO noise
			std::random_device rd;  // Will be used to obtain a seed for the random number engine
			std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
			std::uniform_real_distribution<f32> dis(0.f, 1.f);	// we will unpack to [-1, 1] in shader

			const u32 noiseSamplePerDim = 4;
			std::vector<DirectX::SimpleMath::Vector4> randomDirections;
			for (auto i = 0; i < noiseSamplePerDim * noiseSamplePerDim; ++i)
			{
				DirectX::SimpleMath::Vector3 vec = { dis(gen), dis(gen), 0.f };
				vec.Normalize();
				randomDirections.push_back({ vec.x, vec.y, vec.z, 0.f });
			}
			auto noise = TextureDesc(MemoryType::Default, DXGI_FORMAT_R32G32B32A32_FLOAT, noiseSamplePerDim, noiseSamplePerDim, 1)
				.SetMipLevels(1);
			m_ssaoNoise = m_rd->CreateTexture(noise);

			auto rowPitch = noiseSamplePerDim * sizeof(randomDirections[0]);
			m_perFrameUploadCtx->PushUploadToTexture(m_ssaoNoise, 0, { 0, 0, 0 },
				randomDirections.data(), DXGI_FORMAT_R32G32B32A32_FLOAT,
				noiseSamplePerDim, noiseSamplePerDim, 1,
				(u32)rowPitch);

			// Create 64 samples
			std::uniform_real_distribution<f32> dis2(-1.f, 1.f);

			const u32 hemiSamples = 64;
			std::vector<DirectX::SimpleMath::Vector4> randomSamples;
			// Generate points on the hemisphere
			for (auto i = 0; i < hemiSamples; ++i)
			{
				DirectX::SimpleMath::Vector3 vec = { dis2(gen), dis2(gen), dis2(gen) };
				vec.Normalize();
				randomSamples.push_back({ vec.x, vec.y, vec.z, 0.f });
			}
			// Scale points to distribute them within the hemisphere (closer towards the center)
			for (auto i = 0; i < hemiSamples; ++i)
			{
				float scale = float(i) / float(hemiSamples);
				scale = std::lerp(0.1f, 0.8f, scale * scale * scale);
				randomSamples[i] *= scale;
			}

			BufferDesc samples(MemoryType::Default, (u32)randomSamples.size() * sizeof(randomSamples[0]), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
			m_ssaoSamples = m_rd->CreateBuffer(samples);

			m_perFrameUploadCtx->PushUpload(m_ssaoSamples, 0, randomSamples.data(), samples.size);

		}

		ImGuiMenuLayer::RegisterDebugWindow("Renderer Debug", [this](bool& open) { SpawnRenderDebugWindow(open); }, false, std::make_pair(DOG::Key::LCtrl, DOG::Key::N));

		m_rg = std::move(std::make_unique<RenderGraph>(m_rd, m_rgResMan.get(), m_bin.get()));

		// Import long-lived resources
		m_rgResMan->ImportTexture(RG_RESOURCE(Backbuffer), m_sc->GetNextDrawSurface(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		m_rgResMan->ImportTexture(RG_RESOURCE(NoiseSSAO), m_ssaoNoise, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_DEST);
		m_rgResMan->ImportBuffer(RG_RESOURCE(SamplesSSAO), m_ssaoSamples, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_DEST);

	}

	Renderer::~Renderer()
	{
		DOG::UI::Destroy();
		Flush();
		m_rg->Clear();
		m_bin->ForceClear();
		m_sc->SetFullscreenState(false, {}); // safeguard to prevent crash if game has not exited fullscreen before exit
	}

	Monitor Renderer::GetMonitor() const
	{
		return m_rd->GetMonitor();
	}

	DXGI_MODE_DESC Renderer::GetMatchingDisplayMode(std::optional<DXGI_MODE_DESC> mode) const
	{
		if (mode)
			return static_cast<Swapchain_DX12*>(m_sc)->GetClosestMatchingDisplayModeDesc(*mode);
		else
			return static_cast<Swapchain_DX12*>(m_sc)->GetDefaultDisplayModeDesc();
	}

	void Renderer::SetMainRenderCamera(const DirectX::XMMATRIX& view, DirectX::XMMATRIX* proj)
	{
		m_viewMat = view;
		m_projMat = proj ? *proj : DirectX::XMMatrixPerspectiveFovLH(80.f * 3.1415f / 180.f, (f32)m_renderWidth / m_renderHeight, 800.f, 0.1f);
		//m_projMat = DirectX::XMMatrixPerspectiveFovLH(80.f * 3.1415f / 180.f, (f32)m_renderWidth / m_renderHeight, 0.1f, 800.f);
	}



	void Renderer::SubmitMesh(Mesh mesh, u32 submesh, MaterialHandle material, const DirectX::SimpleMath::Matrix& world, bool isWeapon)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.mat = material;
		sub.world = world;
		sub.isWeapon = isWeapon;

		if (isWeapon)
			m_weaponSubmission.push_back(sub);
		else
			m_submissions.push_back(sub);
	}

	void Renderer::SubmitMeshNoFaceCulling(Mesh mesh, u32 submesh, MaterialHandle material, const DirectX::SimpleMath::Matrix& world)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.mat = material;
		sub.world = world;
		m_noCullSubmissions.push_back(sub);
	}

	void DOG::gfx::Renderer::SubmitMeshWireframe(Mesh mesh, u32 submesh, MaterialHandle material, const DirectX::SimpleMath::Matrix& world)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.mat = material;
		sub.world = world;
		m_wireframeDraws.push_back(sub);
	}

	void DOG::gfx::Renderer::SubmitMeshWireframeNoFaceCulling(Mesh mesh, u32 submesh, MaterialHandle material, const DirectX::SimpleMath::Matrix& world)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.mat = material;
		sub.world = world;
		m_noCullWireframeDraws.push_back(sub);
	}

	void Renderer::SubmitAnimatedMesh(Mesh mesh, u32 submesh, MaterialHandle material, const DirectX::SimpleMath::Matrix& world, u32 jointOffset)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.mat = material;
		sub.world = world;
		sub.jointOffset = jointOffset;
		sub.animated = true;
		m_animatedDraws.push_back(sub);
	}

	void DOG::gfx::Renderer::SubmitSingleSidedShadowMesh(u32 shadowID, Mesh mesh, u32 submesh, const DirectX::SimpleMath::Matrix& world, bool animated, u32 jointOffset)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.world = world;
		sub.animated = animated;
		sub.jointOffset = jointOffset;

		const auto& caster = m_activeShadowCasters[shadowID];
		m_singleSidedShadowDraws[caster.singleSidedBucket].push_back(sub);
	}

	void DOG::gfx::Renderer::SubmitDoubleSidedShadowMesh(u32 shadowID, Mesh mesh, u32 submesh, const DirectX::SimpleMath::Matrix& world, bool animated, u32 jointOffset)
	{
		RenderSubmission sub{};
		sub.mesh = mesh;
		sub.submesh = submesh;
		sub.world = world;
		sub.animated = animated;
		sub.jointOffset = jointOffset;

		const auto& caster = m_activeShadowCasters[shadowID];
		m_doubleSidedShadowDraws[caster.doubleSidedBucket].push_back(sub);
	}

	void DOG::gfx::Renderer::SubmitEmitters(const std::vector<ParticleEmitter>& emitters)
	{
		m_particleBackend->UploadEmitters(emitters);
	}

	std::optional<u32> DOG::gfx::Renderer::RegisterSpotlight(const ActiveSpotlight& data)
	{
		std::optional<u32> id;

		if (data.shadow)
		{
			id = (u32)m_activeShadowCasters.size();
			// Reserve single sided and double sided buckets for this shadow caster
			m_activeShadowCasters.push_back({ m_nextSingleSidedShadowBucket++, m_nextDoubleSidedShadowBucket++ });
		}
		m_activeSpotlights.push_back(data);
		return id;
	}

	void Renderer::Update(f32 dt)
	{
		m_jointMan->UpdateJoints();
		m_globalLightTable->FinalizeUpdates();

		PostProcess::Get().Update(dt);


		// Update per frame data
		{
			m_pfData.viewMatrix = m_viewMat;
			m_pfData.viewMatrix.Invert(m_pfData.invViewMatrix);
			m_pfData.projMatrix = m_projMat;
			m_pfData.projMatrix.Invert(m_pfData.invProjMatrix);
			m_pfData.time += dt;
			m_pfData.deltaTime = dt;

			// Set light data
			m_pfData.pointLightOffsets.staticOffset = m_globalLightTable->GetChunkOffset(LightType::Point, LightUpdateFrequency::Never);
			m_pfData.pointLightOffsets.infreqOffset = m_globalLightTable->GetChunkOffset(LightType::Point, LightUpdateFrequency::Sometimes);
			m_pfData.pointLightOffsets.dynOffset = m_globalLightTable->GetChunkOffset(LightType::Point, LightUpdateFrequency::PerFrame);

			m_pfData.spotLightOffsets.staticOffset = m_globalLightTable->GetChunkOffset(LightType::Spot, LightUpdateFrequency::Never);
			m_pfData.spotLightOffsets.infreqOffset = m_globalLightTable->GetChunkOffset(LightType::Spot, LightUpdateFrequency::Sometimes);
			m_pfData.spotLightOffsets.dynOffset = m_globalLightTable->GetChunkOffset(LightType::Spot, LightUpdateFrequency::PerFrame);

			m_pfData.areaLightOffsets.staticOffset = m_globalLightTable->GetChunkOffset(LightType::Area, LightUpdateFrequency::Never);
			m_pfData.areaLightOffsets.infreqOffset = m_globalLightTable->GetChunkOffset(LightType::Area, LightUpdateFrequency::Sometimes);
			m_pfData.areaLightOffsets.dynOffset = m_globalLightTable->GetChunkOffset(LightType::Area, LightUpdateFrequency::PerFrame);

			// Get camera position
			DirectX::XMVECTOR tmp;
			auto invVm = DirectX::XMMatrixInverse(&tmp, m_viewMat);
			auto pos = invVm.r[3];
			DirectX::XMFLOAT3 posFloat3;
			DirectX::XMStoreFloat3(&posFloat3, pos);
			m_pfData.camPos = { posFloat3.x, posFloat3.y, posFloat3.z, 0.0f };

			m_pfDataTable->RequestUpdate(m_pfHandle, &m_pfData, sizeof(m_pfData));

			// Get offset after update
			m_currPfDescriptor = m_pfDataTable->GetLocalOffset(m_pfHandle);
			m_globalEffectData.perFrameTableOffset = &m_currPfDescriptor;
		}






		m_globalLightTable->SendCopyRequests(*m_perFrameUploadCtx);
		m_globalMaterialTable->SendCopyRequests(*m_perFrameUploadCtx);
		m_pfDataTable->SendCopyRequests(*m_perFrameUploadCtx);


		// Resolve any per frame copies from CPU
		{
			ZoneNamedN(FrameCopyResolve, "Frame Copies", true);
			m_frameCopyReceipt = m_perFrameUploadCtx->SubmitCopies();
		}
	}

	static bool s_donez = false;
	void Renderer::Render(f32)
	{
		ZoneNamedN(RenderScope, "Render", true);
		MINIPROFILE;

		auto& rg = *m_rg;

		if (!s_donez)
		{
			rg.Clear();
			s_donez = true;
		}

		// Change backbuffer resource for this frame
		m_rgResMan->ChangeImportedTexture(RG_RESOURCE(Backbuffer), m_sc->GetNextDrawSurface());


		// Forward pass to HDR
		{
			struct JointData
			{
				DirectX::XMFLOAT4X4 joints[300];
			};

			struct PerDrawData
			{
				DirectX::XMMATRIX world;
				u32 globalSubmeshID{ UINT_MAX };
				u32 globalMaterialID{ UINT_MAX };
				u32 jointsDescriptor{ UINT_MAX };
			};

			/*Struct to be filled in and passed to shader per light*/
			struct PerLightData
			{
				DirectX::XMFLOAT4X4 view{};
				DirectX::XMFLOAT4X4 proj{};
				DirectX::SimpleMath::Vector4 position;
				DirectX::SimpleMath::Vector3 color;
				float cutoffAngle{ 0.f };
				DirectX::SimpleMath::Vector3 direction;
				float strength{ 0.f };
				bool isShadowCaster{ false };
				u32 isPlayer{ 0 };
				float padding[2]{ 0,0 };
			};

			/*Encompasses all the light datas for spotlights, which we currently limit to 12*/
			struct PerLightDataForShadows
			{
				PerLightData perLightDatas[12];
				u32 actualNrOfSpotlights = 0u;
			};

			/*We can have at most 12 shadow maps available on the GPU at any given time (meaning 12 shadowcasters!)*/
			struct ShadowMapArrayStruct
			{
				u32 shadowMaps[12];
			};

			/*The views on the shadow maps (maximum of 12 currently)*/
			struct PassData
			{
				RGResourceView shadowView;
				RGResourceView localLightBuffer;
			};

			struct ShadowPassData
			{};

			/*
				@todo:
					Still need some way to pre-allocate per draw data prior to render pass.
					Perhaps go through the submissions and collect data --> Upload to GPU (maybe instance it as well?)
					and during forward pass we simply read from it
			*/


			auto drawZPassSubmissions = [&, meshTab = m_globalMeshTable.get(), matTab = m_globalMaterialTable.get(), bonezy = m_jointMan.get(), dynConstants = m_dynConstants.get(), dynConstantsAnimated = m_dynConstantsAnimated.get()](RenderDevice* rd, CommandList cmdl, const std::vector<RenderSubmission>& submissions, bool animated = false) mutable
			{
				for (const auto& sub : submissions)
				{
					auto perDrawHandle = dynConstants->Allocate((u32)std::ceilf(sizeof(PerDrawData) / (float)256), false);
					PerDrawData perDrawData{};
					perDrawData.world = sub.world;
					perDrawData.globalSubmeshID = meshTab->GetSubmeshMD_GPU(sub.mesh, sub.submesh);

					GPUDynamicConstant jointsHandle;
					if (animated)
					{
						JointData jointsData{};
						// Resolve joints
						jointsHandle = dynConstantsAnimated->Allocate((u32)std::ceilf(sizeof(JointData) / (float)256));
						for (size_t i = 0; i < bonezy->m_vsJoints.size(); ++i)
							jointsData.joints[i] = bonezy->m_vsJoints[i];
						std::memcpy(jointsHandle.memory, &jointsData, sizeof(jointsData));
						perDrawData.jointsDescriptor = jointsHandle.globalDescriptor;
					}

					std::memcpy(perDrawHandle.memory, &perDrawData, sizeof(perDrawData));

					auto args = ShaderArgs()
						.AppendConstant(m_globalEffectData.globalDataDescriptor)
						.AppendConstant(m_currPfDescriptor)
						.SetPrimaryCBV(perDrawHandle.buffer, perDrawHandle.bufferOffset)
						.AppendConstant(sub.jointOffset);


					rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Graphics, args);

					auto sm = meshTab->GetSubmeshMD_CPU(sub.mesh, sub.submesh);
					rd->Cmd_DrawIndexed(cmdl, sm.indexCount, 1, sm.indexStart, 0, 0);
				}
			};


			auto drawSubmissions = [&, meshTab = m_globalMeshTable.get(), matTab = m_globalMaterialTable.get(), bonezy = m_jointMan.get(), dynConstants = m_dynConstants.get(), dynConstantsAnimated = m_dynConstantsAnimated.get()](RenderDevice* rd, CommandList cmdl, const std::vector<RenderSubmission>& submissions, u32 localLightBuffers, u32 perLightHandle, u32 shadowHandle, bool animated = false, bool wireframe = false) mutable
			{	
				for (const auto& sub : submissions)
				{
					auto perDrawHandle = dynConstants->Allocate((u32)std::ceilf(sizeof(PerDrawData) / (float)256), false);
					PerDrawData perDrawData{};
					perDrawData.world = sub.world;
					perDrawData.globalSubmeshID = meshTab->GetSubmeshMD_GPU(sub.mesh, sub.submesh);
					perDrawData.globalMaterialID = matTab->GetMaterialIndex(sub.mat);

					GPUDynamicConstant jointsHandle;
					if (animated)
					{
						JointData jointsData{};
						// Resolve joints
						jointsHandle = dynConstantsAnimated->Allocate((u32)std::ceilf(sizeof(JointData) / (float)256));
						for (size_t i = 0; i < bonezy->m_vsJoints.size(); ++i)
							jointsData.joints[i] = bonezy->m_vsJoints[i];
						std::memcpy(jointsHandle.memory, &jointsData, sizeof(jointsData));
						perDrawData.jointsDescriptor = jointsHandle.globalDescriptor;
					}

					std::memcpy(perDrawHandle.memory, &perDrawData, sizeof(perDrawData));
					u32 renderSettingsFlag = 0;
					if (m_graphicsSettings.lit) renderSettingsFlag |= DEBUG_SETTING_LIT;
					if (m_graphicsSettings.lightCulling) renderSettingsFlag |= DEBUG_SETTING_LIGHT_CULLING;
					if (m_graphicsSettings.visualizeLightCulling) renderSettingsFlag |= DEBUG_SETTING_LIGHT_CULLING_VISUALIZATION;
					auto args = ShaderArgs()
						.AppendConstant(m_globalEffectData.globalDataDescriptor)
						.AppendConstant(m_currPfDescriptor)
						.SetPrimaryCBV(perDrawHandle.buffer, perDrawHandle.bufferOffset)
						.AppendConstant(perLightHandle)
						.AppendConstant(shadowHandle)
						.AppendConstant(wireframe ? 1 : 0)
						.AppendConstant(renderSettingsFlag)
						.AppendConstant(sub.jointOffset)
						.AppendConstant(m_renderWidth)
						.AppendConstant(m_renderHeight)
						.AppendConstant(localLightBuffers)
						.AppendConstant(sub.isWeapon ? 1 : 0);

					rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Graphics, args);

					auto sm = meshTab->GetSubmeshMD_CPU(sub.mesh, sub.submesh);
					rd->Cmd_DrawIndexed(cmdl, sm.indexCount, 1, sm.indexStart, 0, 0);
				}
			};
		
			auto shadowDrawSubmissions = [&, meshTab = m_globalMeshTable.get(), matTab = m_globalMaterialTable.get(), bonezy = m_jointMan.get(), dynConstants = m_dynConstants.get(), dynConstantsAnimated = m_dynConstantsAnimatedShadows.get()](
				RenderDevice* rd, CommandList cmdl, const std::vector<RenderSubmission>& submissions, u32 smIdx, const ShadowCaster& caster, bool animated = false, bool wireframe = false) mutable
			{
				auto perLightHandle = dynConstants->Allocate((u32)std::ceilf(sizeof(PerLightData) / (float)256));
				PerLightData perLightData{};
				perLightData.view = caster.viewMat;
				perLightData.proj = caster.projMat;
				std::memcpy(perLightHandle.memory, &perLightData, sizeof(perLightData));

				for (const auto& sub : submissions)
				{
					auto perDrawHandle = dynConstants->Allocate((u32)std::ceilf(sizeof(PerDrawData) / (float)256), false);
					PerDrawData perDrawData{};
					perDrawData.world = sub.world;
					perDrawData.globalSubmeshID = meshTab->GetSubmeshMD_GPU(sub.mesh, sub.submesh);
					perDrawData.globalMaterialID = 0;

					if (sub.animated || animated)
					{
						// Resolve joints
						JointData jointsData{};
						auto jointsHandle = dynConstantsAnimated->Allocate((u32)std::ceilf(sizeof(JointData) / (float)256));
						for (size_t i = 0; i < bonezy->m_vsJoints.size(); ++i)
							jointsData.joints[i] = bonezy->m_vsJoints[i];
						std::memcpy(jointsHandle.memory, &jointsData, sizeof(jointsData));
						perDrawData.jointsDescriptor = jointsHandle.globalDescriptor;
					}

					std::memcpy(perDrawHandle.memory, &perDrawData, sizeof(perDrawData));

					auto args = ShaderArgs()
						.AppendConstant(m_globalEffectData.globalDataDescriptor)
						.AppendConstant(m_currPfDescriptor)
						.SetPrimaryCBV(perDrawHandle.buffer, perDrawHandle.bufferOffset)
						.AppendConstant(perLightHandle.globalDescriptor)
						.AppendConstant(wireframe ? 1 : 0)
						.AppendConstant(smIdx)
						.AppendConstant(sub.jointOffset);

					rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Graphics, args);

					auto sm = meshTab->GetSubmeshMD_CPU(sub.mesh, sub.submesh);
					rd->Cmd_DrawIndexed(cmdl, sm.indexCount, 1, sm.indexStart, 0, 0);
				}
			};


			rg.AddPass<PassData>("Z PrePass",
				[&](PassData&, RenderGraph::PassBuilder& builder)
				{
					builder.DeclareTexture(RG_RESOURCE(MainDepth), RGTextureDesc::DepthWrite2D(DepthFormat::D32, m_renderWidth, m_renderHeight));

					builder.WriteDepthStencil(RG_RESOURCE(MainDepth), RenderPassAccessType::ClearPreserve,
						TextureViewDesc(ViewType::DepthStencil, TextureViewDimension::Texture2D, DXGI_FORMAT_D32_FLOAT));
				},
				[&, dynConstants = m_dynConstants.get(), dynConstantsTemp = m_dynConstantsTemp.get(), drawFunc = drawZPassSubmissions](const PassData&, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources&) mutable
				{
					rd->Cmd_SetViewports(cmdl, m_globalEffectData.defRenderVPs);
					rd->Cmd_SetScissorRects(cmdl, m_globalEffectData.defRenderScissors);

					rd->Cmd_SetIndexBuffer(cmdl, m_globalEffectData.meshTable->GetIndexBuffer());

					rd->Cmd_SetPipeline(cmdl, m_zPrePassPipe);
					drawFunc(rd, cmdl, m_submissions);
					drawFunc(rd, cmdl, m_animatedDraws, true);

					rd->Cmd_SetPipeline(cmdl, m_zPrePassPipeNoCull);
					drawFunc(rd, cmdl, m_noCullSubmissions);

					rd->Cmd_SetPipeline(cmdl, m_zPrePassPipeWirefram);
					drawFunc(rd, cmdl, m_wireframeDraws);

					rd->Cmd_SetPipeline(cmdl, m_zPrePassPipeWirefram);
					drawFunc(rd, cmdl, m_noCullWireframeDraws);
				});



			rg.AddPass<ShadowPassData>("Shadow Pass",
				[&](ShadowPassData&, RenderGraph::PassBuilder& builder)
				{
					builder.DeclareTexture(RG_RESOURCE(ShadowDepth), RGTextureDesc::DepthWrite2D(DepthFormat::D32, 1024, 1024, m_shadowMapCapacity));
					builder.WriteDepthStencil(RG_RESOURCE(ShadowDepth), RenderPassAccessType::ClearPreserve,
						TextureViewDesc(ViewType::DepthStencil, TextureViewDimension::Texture2D_Array, DXGI_FORMAT_D32_FLOAT)
						.SetArrayRange(0, m_shadowMapCapacity));

				},
				[&, shadowDrawFunc = shadowDrawSubmissions](const ShadowPassData&, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources&) mutable
				{
					rd->Cmd_SetViewports(cmdl, Viewports().Append(0.f, 0.f, 1024.f, 1024.f));
					rd->Cmd_SetScissorRects(cmdl, ScissorRects().Append(0, 0, 1024, 1024));

					rd->Cmd_SetIndexBuffer(cmdl, m_globalEffectData.meshTable->GetIndexBuffer());

					// Fills shadowmaps chronologically
					rd->Cmd_SetPipeline(cmdl, m_shadowPipe);
					u32 nextMap = 0;
					for (u32 i = 0; i < m_activeSpotlights.size(); ++i)
					{
						if (m_activeSpotlights[i].shadow)
						{
							//shadowDrawFunc(rd, cmdl, m_shadowSubmissions, nextMap++, m_activeSpotlights[i].shadow.value());
							shadowDrawFunc(rd, cmdl, m_singleSidedShadowDraws[m_activeShadowCasters[i].singleSidedBucket], nextMap++, m_activeSpotlights[i].shadow.value());
						}
					}

					// Render the shady modular blocks..
					rd->Cmd_SetPipeline(cmdl, m_shadowPipeNoCull);
					nextMap = 0;
					for (u32 i = 0; i < m_activeSpotlights.size(); ++i)
					{
						if (m_activeSpotlights[i].shadow)
						{
							//shadowDrawFunc(rd, cmdl, m_shadowSubmissionsNoCull, nextMap++, m_activeSpotlights[i].shadow.value());
							shadowDrawFunc(rd, cmdl, m_doubleSidedShadowDraws[m_activeShadowCasters[i].doubleSidedBucket], nextMap++, m_activeSpotlights[i].shadow.value());
						}
					}

				});
			if(m_graphicsSettings.lightCulling)
				m_tiledLightCuller->Add(rg);

			rg.AddPass<PassData>("Forward Pass",
				[&](PassData& p, RenderGraph::PassBuilder& builder)
				{
					builder.DeclareTexture(RG_RESOURCE(LitHDR), RGTextureDesc::RenderTarget2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_renderWidth, m_renderHeight)
						.AddFlag(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
					builder.DeclareTexture(RG_RESOURCE(MainNormals), RGTextureDesc::RenderTarget2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_renderWidth, m_renderHeight));

					p.shadowView = builder.ReadResource(RG_RESOURCE(ShadowDepth), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D_Array, DXGI_FORMAT_R32_FLOAT)
						.SetArrayRange(0, m_shadowMapCapacity));

					builder.WriteRenderTarget(RG_RESOURCE(LitHDR), RenderPassAccessType::ClearPreserve,
						TextureViewDesc(ViewType::RenderTarget, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

					// Write normals
					builder.WriteRenderTarget(RG_RESOURCE(MainNormals), RenderPassAccessType::ClearPreserve,
						TextureViewDesc(ViewType::RenderTarget, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

					builder.ReadDepthStencil(RG_RESOURCE(MainDepth), TextureViewDesc(ViewType::DepthStencil, TextureViewDimension::Texture2D, DXGI_FORMAT_D32_FLOAT).SetDepthReadOnly());

					if (m_graphicsSettings.lightCulling)
					{
						auto groupCount = static_cast<TiledLightCullingEffect*>(m_tiledLightCuller.get())->GetGroupCount();
						p.localLightBuffer = builder.ReadResource(RG_RESOURCE(LocalLightBuf), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, BufferViewDesc(ViewType::ShaderResource, 0, sizeof(TiledLightCullingEffect::LocalLightBufferLayout), groupCount.x * groupCount.y));
					}
				},
				[&, dynConstants = m_dynConstants.get(), dynConstantsTemp = m_dynConstantsTemp.get(), drawFunc = drawSubmissions](const PassData& p, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources& resources) mutable
				{
					//std::cout << "Doing forward pass with: " << passData.somethingAllocated << "\n";

					rd->Cmd_SetViewports(cmdl, m_globalEffectData.defRenderVPs);
					rd->Cmd_SetScissorRects(cmdl, m_globalEffectData.defRenderScissors);

					rd->Cmd_SetIndexBuffer(cmdl, m_globalEffectData.meshTable->GetIndexBuffer());

					rd->Cmd_SetPipeline(cmdl, m_meshPipe);

					PerLightDataForShadows perLightData{};
					ShadowMapArrayStruct shadowMapArrayStruct{};
					auto perLightHandle = dynConstantsTemp->Allocate((u32)std::ceilf(sizeof(PerLightDataForShadows) / (float)256));
					auto shadowHandle = dynConstants->Allocate((u32)std::ceilf(sizeof(ShadowMapArrayStruct) / float(256)));
					for (size_t i{ 0u }; i < m_activeSpotlights.size(); ++i)
					{
						const auto& data = m_activeSpotlights[i];

						perLightData.perLightDatas[i].position = data.position;
						perLightData.perLightDatas[i].color = { data.color.x, data.color.y, data.color.z, };
						perLightData.perLightDatas[i].direction = data.direction;
						perLightData.perLightDatas[i].cutoffAngle = data.cutoffAngle;
						perLightData.perLightDatas[i].strength = data.strength;
						perLightData.perLightDatas[i].isPlayer = data.isPlayerLight ? 1 : 0;

						if (data.shadow != std::nullopt)
						{
							perLightData.perLightDatas[i].isShadowCaster = true;
							perLightData.perLightDatas[i].view = data.shadow->viewMat;
							perLightData.perLightDatas[i].proj = data.shadow->projMat;
							shadowMapArrayStruct.shadowMaps[i] = resources.GetView(p.shadowView);
						}
					}

					perLightData.actualNrOfSpotlights = (u32)m_activeSpotlights.size();
					std::memcpy(perLightHandle.memory, &perLightData, sizeof(perLightData));
					std::memcpy(shadowHandle.memory, &shadowMapArrayStruct, sizeof(shadowMapArrayStruct));

					u32 localLightBufferIndex = m_graphicsSettings.lightCulling ? resources.GetView(p.localLightBuffer) : -1;
					drawFunc(rd, cmdl, m_submissions, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor);
					drawFunc(rd, cmdl, m_animatedDraws, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor, true);
					//drawFunc(rd, cmdl, m_weaponSubmission, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor);
					

					rd->Cmd_SetPipeline(cmdl, m_meshPipeNoCull);
					drawFunc(rd, cmdl, m_noCullSubmissions, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor);

					rd->Cmd_SetPipeline(cmdl, m_meshPipeWireframe);
					drawFunc(rd, cmdl, m_wireframeDraws, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor, false, true);

					rd->Cmd_SetPipeline(cmdl, m_meshPipeWireframeNoCull);
					drawFunc(rd, cmdl, m_noCullWireframeDraws, localLightBufferIndex, false, true);

					rd->Cmd_SetPipeline(cmdl, m_weaponMeshPipe);
					drawFunc(rd, cmdl, m_weaponSubmission, localLightBufferIndex, perLightHandle.globalDescriptor, shadowHandle.globalDescriptor);
				});
		}

		m_damageDiskEffect->Add(rg);

		// Generate SSAO
		{
			struct PassData
			{
				RGResourceView noise, samples;
				RGResourceView depth;
				RGResourceView nor;
				RGResourceView aoOut;
			};

			rg.AddPass<PassData>("SSAO Pass",
				[&](PassData& passData, RenderGraph::PassBuilder& builder)
				{
					// Compute read access
					passData.depth = builder.ReadResource(RG_RESOURCE(MainDepth), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R32_FLOAT));
					passData.nor = builder.ReadResource(RG_RESOURCE(MainNormals), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));
					passData.noise = builder.ReadResource(RG_RESOURCE(NoiseSSAO), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R32G32B32A32_FLOAT));
					passData.samples = builder.ReadResource(RG_RESOURCE(SamplesSSAO), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						BufferViewDesc(ViewType::ShaderResource, 0, sizeof(DirectX::SimpleMath::Vector4), 64));

					// screen space AO texture
					builder.DeclareTexture(RG_RESOURCE(AmbientOcclusion), RGTextureDesc::ReadWrite2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_renderWidth, m_renderHeight));
					passData.aoOut = builder.ReadWriteTarget(RG_RESOURCE(AmbientOcclusion),
						TextureViewDesc(ViewType::UnorderedAccess, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));
				},
				[&](const PassData& passData, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources& resources)
				{
					if (m_graphicsSettings.ssao)
					{

						rd->Cmd_SetPipeline(cmdl, m_ssaoPipe);
						auto args = ShaderArgs()
							.AppendConstant(m_globalEffectData.globalDataDescriptor)
							.AppendConstant(m_currPfDescriptor)
							.AppendConstant(m_renderWidth)
							.AppendConstant(m_renderHeight)
							.AppendConstant(resources.GetView(passData.aoOut))
							.AppendConstant(resources.GetView(passData.depth))
							.AppendConstant(resources.GetView(passData.nor))
							.AppendConstant(resources.GetView(passData.noise))
							.AppendConstant(resources.GetView(passData.samples));
						rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Compute, args);

						rd->Cmd_ClearUnorderedAccessFLOAT(cmdl,
							resources.GetTextureView(passData.aoOut), { 0.f, 0.f, 0.f, 1.f }, ScissorRects().Append(0, 0, m_renderWidth, m_renderHeight));

						// assuming 64 threads per group --> 64 threads per wavefrom, warp is 32 --> use 64
						// we are using 8x8 thread groups
						auto xGroup = (u32)std::ceilf(m_renderWidth / 32.f);
						auto yGroup = (u32)std::ceilf(m_renderHeight / 32.f);
						rd->Cmd_Dispatch(cmdl, xGroup, yGroup, 1);
					}
					else
					{
						rd->Cmd_ClearUnorderedAccessFLOAT(cmdl,
							resources.GetTextureView(passData.aoOut), { 1.f, 1.f, 1.f, 1.f }, ScissorRects().Append(0, 0, m_renderWidth, m_renderHeight));
					}
				});
		}

		// Box blur
		{
			struct PassData
			{
				RGResourceView input;
				RGResourceView output;

			};

			rg.AddPass<PassData>("SSAO Blur Vertical",
				[&](PassData& passData, RenderGraph::PassBuilder& builder)
				{
					builder.DeclareTexture(RG_RESOURCE(AOBlurredUnfinished), RGTextureDesc::ReadWrite2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_renderWidth / 2, m_renderHeight / 2));

					passData.input = builder.ReadResource(RG_RESOURCE(AmbientOcclusion), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));
					passData.output = builder.ReadWriteTarget(RG_RESOURCE(AOBlurredUnfinished),
						TextureViewDesc(ViewType::UnorderedAccess, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

				},
				[&](const PassData& passData, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources& resources)
				{
					// clear
					rd->Cmd_ClearUnorderedAccessFLOAT(cmdl,
						resources.GetTextureView(passData.output), { 1.f, 1.f, 1.f, 1.f }, ScissorRects().Append(0, 0, m_renderWidth / 2, m_renderHeight / 2));

					if (m_graphicsSettings.ssao)
					{
						rd->Cmd_SetPipeline(cmdl, m_boxBlurPipe);

						{
							auto args = ShaderArgs()
								.AppendConstant(m_globalEffectData.globalDataDescriptor)
								.AppendConstant(m_currPfDescriptor)
								.AppendConstant((u32)(m_renderWidth / 2.f))
								.AppendConstant((u32)(m_renderHeight / 2.f))
								.AppendConstant(resources.GetView(passData.input))
								.AppendConstant(resources.GetView(passData.output))
								.AppendConstant(1)
								.AppendConstant(2);		// Downscale factor
							rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Compute, args);

							// assuming 64 threads per group --> 64 threads per wavefrom, warp is 32 --> use 64
							// we are using 8x8 thread groups
							// Using gather method
							auto xGroup = (u32)std::ceilf(m_renderWidth / 2.f / 8.f);
							auto yGroup = (u32)std::ceilf(m_renderHeight / 2.f / 8.f);
							rd->Cmd_Dispatch(cmdl, xGroup, yGroup, 1);
						}
					}
				});

			rg.AddPass<PassData>("SSAO Blur Horizontal",
				[&](PassData& passData, RenderGraph::PassBuilder& builder)
				{
					builder.DeclareTexture(RG_RESOURCE(AOBlurred), RGTextureDesc::ReadWrite2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_renderWidth / 2, m_renderHeight / 2));

					passData.input = builder.ReadResource(RG_RESOURCE(AOBlurredUnfinished), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));
					passData.output = builder.ReadWriteTarget(RG_RESOURCE(AOBlurred),
						TextureViewDesc(ViewType::UnorderedAccess, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

				},
				[&](const PassData& passData, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources& resources)
				{
					// clear
					rd->Cmd_ClearUnorderedAccessFLOAT(cmdl,
						resources.GetTextureView(passData.output), { 1.f, 1.f, 1.f, 1.f }, ScissorRects().Append(0, 0, m_renderWidth / 2, m_renderHeight / 2));

					if (m_graphicsSettings.ssao)
					{
						rd->Cmd_SetPipeline(cmdl, m_boxBlurPipe);
						{
							auto args = ShaderArgs()
								.AppendConstant(m_globalEffectData.globalDataDescriptor)
								.AppendConstant(m_currPfDescriptor)
								.AppendConstant((u32)(m_renderWidth / 2.f))
								.AppendConstant((u32)(m_renderHeight / 2.f))
								.AppendConstant(resources.GetView(passData.input))
								.AppendConstant(resources.GetView(passData.output))
								.AppendConstant(0)
								.AppendConstant(1);
							rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Compute, args);

							// assuming 64 threads per group --> 64 threads per wavefrom, warp is 32 --> use 64
							// we are using 8x8 thread groups
							// Using gather method
							auto xGroup = (u32)std::ceilf(m_renderWidth / 2.f / 8.f);
							auto yGroup = (u32)std::ceilf(m_renderHeight / 2.f / 8.f);
							rd->Cmd_Dispatch(cmdl, xGroup, yGroup, 1);
						}

					}
				});
		}


		// Test compute on Lit HDR
		// Uncomment to enable the test compute effect!
		//m_testComputeEffect->Add(rg);

		m_particleBackend->AddEffect(*m_rg);

		if (m_bloomEffect)
			m_bloomEffect->Add(rg);

		//m_tiledLightCullerVisualization->Add(rg);

		// Blit HDR to LDR
		{
			struct PassData
			{
				RGResourceView litHDRView;
				RGResourceView ao;
				RGResourceView bloom;
			};
			rg.AddPass<PassData>("Blit to HDR Pass",
				[&](PassData& passData, RenderGraph::PassBuilder& builder)
				{

					passData.ao = builder.ReadResource(RG_RESOURCE(AOBlurred), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

					if (m_bloomEffect)
					{
						passData.bloom = builder.ReadResource(RG_RESOURCE(FinalBloom), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));
					}


					passData.litHDRView = builder.ReadResource(RG_RESOURCE(LitHDR), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						TextureViewDesc(ViewType::ShaderResource, TextureViewDimension::Texture2D, DXGI_FORMAT_R16G16B16A16_FLOAT));

					builder.WriteRenderTarget(RG_RESOURCE(Backbuffer), RenderPassAccessType::ClearPreserve,
						TextureViewDesc(ViewType::RenderTarget, TextureViewDimension::Texture2D, DXGI_FORMAT_R8G8B8A8_UNORM));
				},
				[&](const PassData& passData, RenderDevice* rd, CommandList cmdl, RenderGraph::PassResources& resources)
				{
					rd->Cmd_SetViewports(cmdl, m_globalEffectData.bbVP);
					rd->Cmd_SetScissorRects(cmdl, m_globalEffectData.bbScissor);

					rd->Cmd_SetPipeline(cmdl, m_pipe);
					u32 gammaCast = *((u32*)&m_graphicsSettings.gamma);


					rd->Cmd_UpdateShaderArgs(cmdl, QueueType::Graphics, ShaderArgs()
						.AppendConstant(m_graphicsSettings.ssao ? resources.GetView(passData.ao) : UINT_MAX)
						.AppendConstant(resources.GetView(passData.litHDRView))
						.AppendConstant(gammaCast)
						.AppendConstant(m_bloomEffect ? resources.GetView(passData.bloom) : UINT_MAX));

					rd->Cmd_Draw(cmdl, 3, 1, 0, 0);
				});
		}

		// Final ImGUI pass
		m_imGUIEffect->Add(rg);

		{
			ZoneNamedN(RGBuildScope, "RG Building", true);
			//rg.Build();
			rg.TryBuild();
		}

		{
			ZoneNamedN(RGExecuteScope, "RG Execution", true);
			m_frameSyncs[m_currFrameIdx] = rg.Execute(m_frameCopyReceipt, true);
		}
		auto instance = DOG::UI::Get();
		instance->GetBackend()->BeginFrame();
		instance->DrawUI();
		instance->GetBackend()->EndFrame();
	}

	void Renderer::OnResize(u32 clientWidth, u32 clientHeight)
	{
		auto instance = DOG::UI::Get();
		instance->FreeResize();
		if (clientWidth != 0 && clientHeight != 0)
		{
			m_globalEffectData.bbScissor = ScissorRects().Append(0, 0, clientWidth, clientHeight);
			m_globalEffectData.bbVP = Viewports().Append(0.f, 0.f, (f32)clientWidth, (f32)clientHeight);
		}

		m_sc->OnResize(clientWidth, clientHeight);
		instance->Resize(clientWidth, clientHeight);

		UIRebuild(clientHeight, clientWidth);


	}

	WindowMode DOG::gfx::Renderer::GetFullscreenState() const
	{
		return m_sc->GetFullscreenState() ? WindowMode::FullScreen : WindowMode::Windowed;
	}

	void Renderer::SetGraphicsSettings(GraphicsSettings requestedSettings)
	{
		assert(requestedSettings.displayMode);

		Flush();
		s_donez = false;
		std::cout << "changed gfx settings\n";

		if (!requestedSettings.bloom)
		{
			m_bloomEffect = nullptr;
		}
		else if (!m_bloomEffect || m_renderWidth != requestedSettings.renderResolution.x || m_renderHeight != requestedSettings.renderResolution.y)
		{
			m_bloomEffect.reset();
			m_bloomEffect = std::make_unique<Bloom>(m_rgResMan.get(), m_globalEffectData, m_dynConstants.get(), requestedSettings.renderResolution.x, requestedSettings.renderResolution.y);
		}
		if (m_bloomEffect) m_bloomEffect->SetGraphicsSettings(requestedSettings);
		if (m_tiledLightCuller) m_tiledLightCuller->SetGraphicsSettings(requestedSettings);
		m_renderWidth = requestedSettings.renderResolution.x;
		m_renderHeight = requestedSettings.renderResolution.y;
		m_globalEffectData.defRenderScissors = ScissorRects().Append(0, 0, m_renderWidth, m_renderHeight);
		m_globalEffectData.defRenderVPs = Viewports().Append(0.f, 0.f, (f32)m_renderWidth, (f32)m_renderHeight);
		m_sc->SetFullscreenState(requestedSettings.windowMode == WindowMode::FullScreen, *requestedSettings.displayMode);

		m_shadowMapCapacity = requestedSettings.shadowMapCapacity;

		m_singleSidedShadowDraws.resize(requestedSettings.shadowMapCapacity);
		m_doubleSidedShadowDraws.resize(requestedSettings.shadowMapCapacity);

		m_dynConstantsAnimatedShadows = std::make_unique<GPUDynamicConstants>(m_rd, m_bin.get(), 75 * 100 * m_shadowMapCapacity * S_MAX_FIF);


		m_graphicsSettings = requestedSettings;
	}

	GraphicsSettings DOG::gfx::Renderer::GetGraphicsSettings()
	{
		return m_graphicsSettings;
	}

	void Renderer::BeginFrame_GPU()
	{

		WaitForPrevFrame();


		m_dynConstants->Tick();
		m_dynConstantsTemp->Tick();
		m_dynConstantsAnimated->Tick();
		m_dynConstantsAnimatedShadows->Tick();
		m_bin->BeginFrame();
		m_rd->RecycleCommandList(m_cmdl);
		m_cmdl = m_rd->AllocateCommandList();
	}

	void Renderer::EndFrame_GPU(bool)
	{
		EndGUI();
		m_bin->EndFrame();
		m_submissions.clear();
		m_noCullSubmissions.clear();
		m_animatedDraws.clear();
		m_wireframeDraws.clear();
		m_noCullWireframeDraws.clear();
		m_weaponSubmission.clear();
		m_activeSpotlights.clear();

		m_activeShadowCasters.clear();
		for (auto& v : m_singleSidedShadowDraws)
			v.clear();
		for (auto& v : m_doubleSidedShadowDraws)
			v.clear();
		m_nextSingleSidedShadowBucket = m_nextDoubleSidedShadowBucket = 0;

		m_sc->Present(m_graphicsSettings.vSync);

		m_currFrameIdx = (m_currFrameIdx + 1) % S_MAX_FIF;
	}

	void Renderer::Flush()
	{
		m_rd->Flush();
		m_bin->ForceClear();
	}

	void Renderer::BeginGUI()
	{
		m_imgui->BeginFrame();
	}

	void Renderer::EndGUI()
	{
		m_imgui->EndFrame();
	}


	static float s_dir[2] = { 0.f, 1.f };
	static float s_intensity = 2.f;
	static float s_time = 2.f;

	void Renderer::SpawnRenderDebugWindow(bool& open)
	{
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Renderer"))
			{
				open = true;

			}
			ImGui::EndMenu(); // "View"
		}

		if (open)
		{
			//if (ImGui::Begin("Effects Manager", &open))
			//{
			//	bool ssaoState = m_ssaoOn;
			//	ImGui::Checkbox("SSAO", &m_ssaoOn);
			//	if (m_ssaoOn != ssaoState)
			//		s_donez = false;

			//}
			//ImGui::End();


			if (ImGui::Begin("GPU Memory Statistics: Total", &open))
			{
				auto& info = m_rd->GetTotalMemoryInfo().heap[0];
				ImGui::Text("Used allocations: %f (Mb)", info.allocationBytes / 1048576.f);
				ImGui::Text("Memory allocated: %f (Mb)", info.blockBytes / 1048576.f);
				ImGui::Text("Smallest allocation: %f (Mb)", info.smallestAllocation / 1048576.f);
				ImGui::Text("Largest allocation: %f (Mb)", info.largestAllocation / 1048576.f);
			}
			ImGui::End();


			if (ImGui::Begin("GPU Memory Statistics: Render Graph", &open))
			{
				auto& info = m_rgResMan->GetMemoryInfo();
				//auto& info = m_rd->GetTotalMemoryInfo().heap[0];
				ImGui::Text("Used allocations: %f (Mb)", info.allocationBytes / 1048576.f);
				ImGui::Text("Memory allocated: %f (Mb)", info.blockBytes / 1048576.f);
				ImGui::Text("Smallest allocation: %f (Mb)", info.smallestAllocation / 1048576.f);
				ImGui::Text("Largest allocation: %f (Mb)", info.largestAllocation / 1048576.f);


				if (ImGui::Button("Render Graph Rebuild"))
					s_donez = false;
			}
			ImGui::End();


			if (ImGui::Begin("Damage Disk Settings"))
			{
				ImGui::SliderFloat2("Direction", s_dir, -1.f, 1.f);
				ImGui::SliderFloat("Intensity", &s_intensity, 0.f, 3.f);
				ImGui::SliderFloat("Time", &s_time, 0.f, 5.f);

				if (ImGui::Button("Instantiate damage disk"))
				{
					PostProcess::Get().InstantiateDamageDisk({ s_dir[0], s_dir[1] }, s_intensity, s_time);

				}

			}
			ImGui::End();

		}
	}

	LRESULT Renderer::WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (m_imgui)
			return m_imgui->WinProc(hwnd, uMsg, wParam, lParam);
		else
			return false;
	}

	void DOG::gfx::Renderer::WaitForPrevFrame()
	{
		MINIPROFILE;
		if (m_frameSyncs[m_currFrameIdx])
			m_rd->WaitForGPU(m_frameSyncs[m_currFrameIdx].value());
	}

}



