#include "RGResourceRepo.h"
#include "../../RHI/RenderDevice.h"
#include "../../Rendering/GPUGarbageBin.h"

namespace DOG::gfx
{
	RGResourceRepo::RGResourceRepo(RenderDevice* rd, GPUGarbageBin* bin) :
		m_rd(rd),
		m_bin(bin)
	{
		m_textures.resize(1);
	}

	void RGResourceRepo::Tick()
	{
		// Condition for resource deletion
		auto shouldDelete = [this](u64 lastFrameAccess) -> bool
		{
			return m_currFrame > (lastFrameAccess + MAX_UNUSED_RESOURCE_LIFETIME);
		};

		++m_currFrame;
	}

	RGResource RGResourceRepo::DeclareResource(const RGTextureDesc& desc)
	{
		Texture_Storage storage{};
		storage.desc = desc;
		storage.currState = desc.initState;
			
		auto hdl = m_handleAtor.Allocate<RGResource>();
		HandleAllocator::TryInsert(m_textures, storage, HandleAllocator::GetSlot(hdl.handle));
		
		return hdl;
	}

	RGResource RGResourceRepo::ImportResource(Texture tex, D3D12_RESOURCE_STATES initState, D3D12_RESOURCE_STATES outState)
	{
		Texture_Storage storage{};
		storage.realized = tex;
		storage.currState = initState;
		storage.outState = outState;
		storage.desc.initState = initState;
		storage.imported = true;

		auto hdl = m_handleAtor.Allocate<RGResource>();
		HandleAllocator::TryInsert(m_textures, storage, HandleAllocator::GetSlot(hdl.handle));

		return hdl;
	}

	Texture RGResourceRepo::GetTexture(RGResource tex)
	{
		const auto& res = HandleAllocator::TryGet(m_textures, HandleAllocator::GetSlot(tex.handle));
		assert(res.realized);
		return *res.realized;
	}

	std::pair<u32, u32>& RGResourceRepo::GetMutEffectiveLifetime(RGResource tex)
	{
		auto& res = HandleAllocator::TryGet(m_textures, HandleAllocator::GetSlot(tex.handle));
		return res.effectiveLifetime;
	}

	D3D12_RESOURCE_STATES RGResourceRepo::GetState(RGResource tex)
	{
		auto& res = HandleAllocator::TryGet(m_textures, HandleAllocator::GetSlot(tex.handle));
		return res.currState;
	}

	void RGResourceRepo::SetState(RGResource tex, D3D12_RESOURCE_STATES state)
	{
		auto& res = HandleAllocator::TryGet(m_textures, HandleAllocator::GetSlot(tex.handle));
		res.currState = state;
	}

	void RGResourceRepo::TransitionImportedState(CommandList cmdl)
	{
		std::vector<GPUBarrier> barriers;
		for (auto& res : m_textures)
		{
			if (res && res->realized && res->imported && res->enabled)
			{
				barriers.push_back(GPUBarrier::Transition(*res->realized, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, res->currState, res->outState));
				res->enabled = false;
			}
		}
		m_rd->Cmd_Barrier(cmdl, barriers);
	}

	void RGResourceRepo::RealizeResources()
	{
		for (auto& res : m_textures)
		{
			if (res && !res->realized)
			{
				TextureDesc desc(MemoryType::Default, res->desc.format,
					res->desc.width, res->desc.height, res->desc.depth,
					res->desc.flags, res->desc.initState);

				res->realized = m_rd->CreateTexture(desc);
			}
		}
	}

}