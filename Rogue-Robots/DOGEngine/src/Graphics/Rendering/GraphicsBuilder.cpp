#include "GraphicsBuilder.h"
#include "../RHI/RenderDevice.h"
#include "UploadContext.h"

namespace DOG::gfx
{
	GraphicsBuilder::GraphicsBuilder(RenderDevice* rd, UploadContext* dataUpCtx, UploadContext* texUpCtx, MeshTable* meshes, MaterialTable* mats) :
		m_rd(rd),
		m_meshTable(meshes),
		m_matTable(mats),
		m_uploadCtx(dataUpCtx),
		m_texUploadCtx(texUpCtx)
	{
	}

	StaticModel GraphicsBuilder::LoadCustomModel(const MeshTable::MeshSpecification& meshSpec, const std::vector<MaterialTable::MaterialSpecification>& matSpecs)
	{
		// Load mesh
		MeshContainer cont;
		cont = m_meshTable->LoadMesh(meshSpec, *m_uploadCtx);

		// Load material
		std::vector<MaterialHandle> loadedMats;
		loadedMats.reserve(matSpecs.size());
		for (const auto& mat : matSpecs)
			loadedMats.push_back(m_matTable->LoadMaterial(mat, *m_uploadCtx));

		// Execute copies
		m_uploadCtx->SubmitCopies();

		StaticModel ret{};
		ret.mesh = cont;
		ret.mats = std::move(loadedMats);

		return ret;
	}

	Texture GraphicsBuilder::LoadTexture(const MippedTexture2DSpecification& spec)
	{
		// Assuming 32-bit per pixel and SRGB for now (needs to be changed when using Block Compressed textures)
		const DXGI_FORMAT texFormat = spec.srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		const u8 bytesPerPixel = sizeof(u32);
		// D3D12 alignment requirement for textures
		static constexpr u32 TEXALIGNMENT = 512;

		const u32 baseWidth = spec.dataPerMip[0].width;
		const u32 baseHeight = spec.dataPerMip[0].height;

		assert(baseWidth != 0 && baseHeight != 0);

		// Create texture
		auto d = TextureDesc(
			MemoryType::Default, texFormat,
			baseWidth, baseHeight, 1)
			.SetMipLevels((u32)spec.dataPerMip.size());
		Texture tex = m_rd->CreateTexture(d);

		// Load each mip to texture
		for (u32 mip = 0; mip < spec.dataPerMip.size(); ++mip)
		{
			const auto& mipData = spec.dataPerMip[mip];

			// Get aligned row pitch for destination
			const u32 rowPitchOriginal = mipData.width * bytesPerPixel;
			const u32 rowPitchAligned = (1 + ((rowPitchOriginal - 1) / TEXALIGNMENT)) * TEXALIGNMENT;

			m_texUploadCtx->PushUploadToTexture(
				// Target
				tex, mip, { 0, 0, 0 },
				// Source
				(void*)mipData.data.data(),
				texFormat, mipData.width, mipData.height, 1,
				rowPitchAligned
			);
		}

		m_texUploadCtx->SubmitCopies();


		return tex;
	}

	TextureView GraphicsBuilder::CreateTextureView(Texture tex, std::optional<TextureViewDesc> desc)
	{
		if (!desc)
		{
			assert(false);
			// we should switch to optional to utilize D3D12's default view descs!
			//m_rd->CreateView(tex, )
		}
		else
		{
			return m_rd->CreateView(tex, *desc);
		}

		assert(false);
		return TextureView();
	}
}