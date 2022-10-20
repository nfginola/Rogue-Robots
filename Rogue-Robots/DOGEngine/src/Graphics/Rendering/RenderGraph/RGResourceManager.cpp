#include "RGResourceManager.h"
#include "../../RHI/RenderDevice.h"
#include "../GPUGarbageBin.h"

namespace DOG::gfx
{
	RGResourceManager::RGResourceManager(RenderDevice* rd, GPUGarbageBin* bin) :
		m_rd(rd),
		m_bin(bin)
	{
		MemoryPoolDesc d{};
		d.size = 100'000'000;
		m_memoryPool = m_rd->CreateMemoryPool(d);
	}

	RGResourceManager::~RGResourceManager()
	{

	}

	const GPUPoolMemoryInfo& RGResourceManager::GetMemoryInfo() const
	{
		return m_rd->GetPoolMemoryInfo(m_memoryPool);
	}

	void RGResourceManager::DeclareTexture(RGResourceID id, RGTextureDesc desc)
	{
		assert(!m_resources.contains(id));

		RGResourceDeclared decl;
		decl.desc = desc;
		decl.currState = desc.initState;

		auto& res = m_resources[id];
		res.resourceType = RGResourceType::Texture;
		res.variantType = RGResourceVariant::Declared;
		res.variants = decl;
	}

	void RGResourceManager::ImportTexture(RGResourceID id, Texture texture, D3D12_RESOURCE_STATES entryState, D3D12_RESOURCE_STATES exitState)
	{
		assert(!m_resources.contains(id));

		auto& res = m_resources[id];

		RGResourceImported imported;
		imported.importEntryState = entryState;
		imported.currState = entryState;
		imported.importExitState = exitState;

		res.resource = texture.handle;
		res.resourceType = RGResourceType::Texture;
		res.variantType = RGResourceVariant::Imported;
		res.variants = imported;
	}

	void RGResourceManager::FreeImported(RGResourceID id)
	{
		m_resources.erase(id);
	}


	void RGResourceManager::DeclareBuffer(RGResourceID id, RGBufferDesc desc)
	{
		assert(!m_resources.contains(id));

		RGResourceDeclared decl;
		decl.desc = desc;
		decl.currState = desc.initState;

		auto& res = m_resources[id];
		res.variantType = RGResourceVariant::Declared;
		res.resourceType = RGResourceType::Buffer;
		res.variants = decl;
	}

	void RGResourceManager::ImportBuffer(RGResourceID id, Buffer buffer, D3D12_RESOURCE_STATES entryState, D3D12_RESOURCE_STATES exitState)
	{
		assert(!m_resources.contains(id));

		auto& res = m_resources[id];

		RGResourceImported imported;
		imported.importEntryState = entryState;
		imported.currState = entryState;
		imported.importExitState = exitState;

		res.resource = buffer.handle;
		res.variantType = RGResourceVariant::Imported;
		res.resourceType = RGResourceType::Buffer;
		res.variants = imported;
	}

	void RGResourceManager::ChangeImportedTexture(RGResourceID id, Texture texture)
	{
		assert(m_resources.contains(id));
		auto& res = m_resources[id];
		assert(res.resourceType == RGResourceType::Texture);
		res.resource = texture.handle;
	}

	void RGResourceManager::ChangeImportedBuffer(RGResourceID id, Buffer buffer)
	{
		assert(m_resources.contains(id));
		auto& res = m_resources[id];
		assert(res.resourceType == RGResourceType::Buffer);
		res.resource = buffer.handle;
	}

	void RGResourceManager::AliasResource(RGResourceID newID, RGResourceID oldID, RGResourceType type)
	{
		assert(!m_resources.contains(newID));
		assert(m_resources.contains(oldID));

		auto& oldRes = m_resources[oldID];
		assert(oldRes.hasBeenAliased == false);		// Enforce single-alias rule
		oldRes.hasBeenAliased = true;

		RGResourceAliased alias;
		alias.prevID = oldID;
		if (oldRes.variantType == RGResourceVariant::Declared || oldRes.variantType == RGResourceVariant::Imported)
		{
			alias.originalID = oldID;
		}
		else
		{
			const auto& variant = std::get<RGResourceAliased>(oldRes.variants);
			alias.originalID = variant.originalID;
		}

		auto& newRes = m_resources[newID];
		newRes.variantType = RGResourceVariant::Aliased;
		newRes.resourceType = type;
		newRes.variants = alias;
	}

	void RGResourceManager::DeclareProxy(RGResourceID id)
	{
		// Marks ID as unavailable
		auto& res = m_resources[id];
		res.variantType = RGResourceVariant::Proxy;
	}




	void RGResourceManager::ClearDeclaredResources()
	{
		for (const auto& [name, resource] : m_resources)
		{
			if (resource.variantType == RGResourceVariant::Imported)
				m_importedTransfer.push_back({ name, resource });

			// We only release graph created resources (declared)
			if (resource.variantType != RGResourceVariant::Declared)
				continue;

			// Safely free resources later
			if (resource.resourceType == RGResourceType::Texture)
			{
				auto delFunc = [rd = m_rd, gpuResource = resource.resource]()
				{
					rd->FreeTexture(Texture(gpuResource));
				};
				m_bin->PushDeferredDeletion(delFunc);
			}
			else
			{
				auto delFunc = [rd = m_rd, gpuResource = resource.resource]()
				{
					rd->FreeBuffer(Buffer(gpuResource));
				};
				m_bin->PushDeferredDeletion(delFunc);
			}
		}

		m_resources.clear();

		// Place back the imported resources
		for (auto& [name, resource] : m_importedTransfer)
		{
			// Reset
			resource.hasBeenAliased = false;
			resource.usageLifetime = { std::numeric_limits<u32>::max(), std::numeric_limits<u32>::min() };

			// Move
			m_resources[std::move(name)] = std::move(resource);
		}
		m_importedTransfer.clear();
	}

	void RGResourceManager::RealizeResources()
	{
		for (auto& [_, resource] : m_resources)
		{
			// We are only interested in creating resources for Declared resources
			if (resource.variantType != RGResourceVariant::Declared)
				continue;

			/*
				@todo: We eventually want add memory aliasing to Textures
			*/
			const RGResourceDeclared& decl = std::get<RGResourceDeclared>(resource.variants);
			if (resource.resourceType == RGResourceType::Texture)
			{
				const auto& rgDesc = std::get<RGTextureDesc>(decl.desc);

				auto desc = TextureDesc(MemoryType::Default, rgDesc.format,
					rgDesc.width, rgDesc.height, rgDesc.depth,
					rgDesc.flags, rgDesc.initState)
					.SetMipLevels(rgDesc.mipLevels);
				resource.resource = m_rd->CreateTexture(desc, m_memoryPool).handle;
			}
			else
			{
				const auto& rgDesc = std::get<RGBufferDesc>(decl.desc);

				BufferDesc desc(MemoryType::Default, rgDesc.size, rgDesc.flags, rgDesc.initState);
				resource.resource = m_rd->CreateBuffer(desc, m_memoryPool).handle;
			}
		}

		// Assign underlying resource to aliased resources
		for (auto& [_, resource] : m_resources)
		{
			if (resource.variantType != RGResourceVariant::Aliased)
				continue;

			RGResourceAliased& alias = std::get<RGResourceAliased>(resource.variants);
			const auto& original = m_resources.find(alias.originalID)->second;
			resource.resource = original.resource;
		}
	}

	void RGResourceManager::SanitizeAliasingLifetimes()
	{
		for (auto& [_, resource] : m_resources)
		{
			// If not a Declared resource, we are not interested in creating a resource for it
			if (resource.variantType != RGResourceVariant::Aliased)
				continue;

			const RGResourceAliased& decl = std::get<RGResourceAliased>(resource.variants);

			// Parent usage lifetime must be < aliased usage lifetime since parent is consumed when the aliased lifetime starts.
			const auto& res = m_resources.find(decl.prevID)->second;
			const auto& parentLifetime = res.usageLifetime;
			const auto& thisLifetime = resource.usageLifetime;

			const auto parentEnd = parentLifetime.second;
			const auto aliasBegin = thisLifetime.first;
			
			assert(parentEnd <= aliasBegin);
		}
	}

	void RGResourceManager::ImportedResourceExitTransition(CommandList cmdl)
	{
		std::vector<GPUBarrier> barriers;
		for (auto& [_, resource] : m_resources)
		{
			if (resource.variantType != RGResourceVariant::Imported)
				continue;

			RGResourceImported& imported = std::get<RGResourceImported>(resource.variants);
			if (imported.currState == imported.importExitState)
			{
				// Resource already in exit state --> Ready to be used outside of graph
				// We also reset the currState to entry state for next entry
				imported.currState = imported.importEntryState;
				continue;
			}

			/*
				We transition Imported textures to the desired Exit State for external usage, but for the graph,
				we reset its internal resource state to the declared Entry State declared by the user upon resource entry to the graph.
			*/
			if (resource.resourceType == RGResourceType::Texture)
			{
				barriers.push_back(GPUBarrier::Transition(Texture(resource.resource), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, imported.currState, imported.importExitState));
				imported.currState = imported.importEntryState;
			}
			else
			{
				barriers.push_back(GPUBarrier::Transition(Buffer(resource.resource), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, imported.currState, imported.importExitState));
				imported.currState = imported.importEntryState;
			}
		}

		if (!barriers.empty())
			m_rd->Cmd_Barrier(cmdl, barriers);
	}

	void RGResourceManager::DeclaredResourceTransitionToInit(CommandList cmdl)
	{
		std::vector<GPUBarrier> barriers;
		for (auto& [_, resource] : m_resources)
		{
			if (resource.variantType != RGResourceVariant::Declared)
				continue;

			RGResourceDeclared& declared = std::get<RGResourceDeclared>(resource.variants);


			if (resource.resourceType == RGResourceType::Texture)
			{
				RGTextureDesc& d = std::get<RGTextureDesc>(declared.desc);
				if (declared.currState == d.initState)
					continue;
				barriers.push_back(GPUBarrier::Transition(Texture(resource.resource), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, declared.currState, d.initState));
				declared.currState = d.initState;

			}
			else
			{
				RGBufferDesc& d = std::get<RGBufferDesc>(declared.desc);
				if (declared.currState == d.initState)
					continue;
				barriers.push_back(GPUBarrier::Transition(Buffer(resource.resource), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, declared.currState, d.initState));
				declared.currState = d.initState;

			}
			
		}

		if (!barriers.empty())
			m_rd->Cmd_Barrier(cmdl, barriers);
	}

	void RGResourceManager::ResolveLifetime(RGResourceID id, u32 depth)
	{
		// Track start lifetime
		{
			auto& res = m_resources.find(id)->second;
			// Get all the way back to the Declared/Imported resource
			RGResourceManager::RGResource* resource{ &res };
			while (resource->variantType == RGResourceVariant::Aliased)
				resource = &m_resources.find(std::get<RGResourceAliased>(resource->variants).prevID)->second;

			std::pair<u32, u32>* resourceLifetime;
			if (resource->variantType == RGResourceVariant::Declared)
				resourceLifetime = &std::get<RGResourceDeclared>(resource->variants).resourceLifetime;
			else
				resourceLifetime = &std::get<RGResourceImported>(resource->variants).resourceLifetime;

			resourceLifetime->first = (std::min)(depth, resourceLifetime->first);
			resourceLifetime->second = (std::max)(depth, resourceLifetime->second);
		}

		// Track usage lifetime
		{
			auto& usageLifetime = m_resources.find(id)->second.usageLifetime;
			usageLifetime.first = (std::min)(depth, usageLifetime.first);
			usageLifetime.second = (std::max)(depth, usageLifetime.second);
		}
	}




	u64 RGResourceManager::GetResource(RGResourceID id)
	{
		// Aliasing always goes to the original resource
		auto& res = m_resources.find(id)->second;
		if (res.variantType == RGResourceVariant::Aliased)
		{
			RGResourceAliased& aliased = std::get<RGResourceAliased>(res.variants);
			auto& originalRes = m_resources.find(aliased.originalID)->second;

			res.resource = originalRes.resource;		// Update resource
			return res.resource;
		}
		else
		{
			return m_resources.find(id)->second.resource;
		}

	}

	RGResourceType RGResourceManager::GetResourceType(RGResourceID id) const
	{
		return m_resources.find(id)->second.resourceType;
	}

	RGResourceVariant RGResourceManager::GetResourceVariant(RGResourceID id) const
	{
		return m_resources.find(id)->second.variantType;
	}

	D3D12_RESOURCE_STATES RGResourceManager::GetCurrentState(RGResourceID id) const
	{
		const auto& res = m_resources.find(id)->second;

		// Get all the way back to the Declared/Imported resource
		const RGResourceManager::RGResource* resource{ &res };
		while (resource->variantType == RGResourceVariant::Aliased)
			resource = &m_resources.find(std::get<RGResourceAliased>(resource->variants).prevID)->second;

		if (resource->variantType == RGResourceVariant::Declared)
			return std::get<RGResourceDeclared>(resource->variants).currState;
		else 
			return std::get<RGResourceImported>(resource->variants).currState;
	}

	std::pair<u32, u32>& RGResourceManager::GetMutableUsageLifetime(RGResourceID id)
	{
		return m_resources.find(id)->second.usageLifetime;
	}

	std::pair<u32, u32>& RGResourceManager::GetMutableResourceLifetime(RGResourceID id)
	{
		assert(m_resources.contains(id));
		auto& res = m_resources.find(id)->second;

		// Get all the way back to the Declared/Imported resource
		RGResourceManager::RGResource* resource{ &res };
		while (resource->variantType == RGResourceVariant::Aliased)
			resource = &m_resources.find(std::get<RGResourceAliased>(resource->variants).prevID)->second;

		if (resource->variantType == RGResourceVariant::Declared)
			return std::get<RGResourceDeclared>(resource->variants).resourceLifetime;
		else
			return std::get<RGResourceImported>(resource->variants).resourceLifetime;

	}

	void RGResourceManager::SetCurrentState(RGResourceID id, D3D12_RESOURCE_STATES state)
	{
		auto& res = m_resources.find(id)->second;

		// Get all the way back to the Declared/Imported resource
		RGResourceManager::RGResource* resource{ &res };
		while (resource->variantType == RGResourceVariant::Aliased)
			resource = &m_resources.find(std::get<RGResourceAliased>(resource->variants).prevID)->second;

		if (resource->variantType == RGResourceVariant::Declared)
			std::get<RGResourceDeclared>(resource->variants).currState = state;
		else
			std::get<RGResourceImported>(resource->variants).currState = state;
	}
}