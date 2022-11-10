#pragma once
#include "RGTypes.h"
#include "../../RHI/Types/GPUInfo.h"
#include "../../RHI/Types/BarrierDesc.h"
#include "../../RHI/RenderResourceHandles.h"

namespace DOG::gfx
{
	class RenderDevice;
	class GPUGarbageBin;

	class RGResourceManager
	{
		friend class RenderGraph;
	public:
		RGResourceManager(RenderDevice* rd, GPUGarbageBin* bin);
		~RGResourceManager();

		const GPUPoolMemoryInfo& GetMemoryInfo();

		// Discards the resources stored safely and clears map for re-use.
		void ClearDeclaredResources();

		void ImportTexture(RGResourceID id, Texture texture, D3D12_RESOURCE_STATES entryState, D3D12_RESOURCE_STATES exitState);
		void ImportBuffer(RGResourceID id, Buffer buffer, D3D12_RESOURCE_STATES entryState, D3D12_RESOURCE_STATES exitState);

		void FreeImported(RGResourceID id);

		void ChangeImportedTexture(RGResourceID id, Texture texture);
		void ChangeImportedBuffer(RGResourceID id, Buffer buffer);


	private:
		friend class RenderGraph;
		
		// These interfaces are exposed through PassBuilder
		void DeclareTexture(RGResourceID id, RGTextureDesc desc);

		void DeclareBuffer(RGResourceID id, RGBufferDesc desc);

		void AliasResource(RGResourceID newID, RGResourceID oldID, RGResourceType type);
		void DeclareProxy(RGResourceID id);


		// Interface for render graph
		void ResolveMemoryAliases(CommandList list, u32 depLevel);
		void ResolveMemoryAliasesWrap(CommandList list);


	private:

		struct RGResourceDeclared
		{
			std::variant<RGTextureDesc, RGBufferDesc> desc;
			std::pair<u32, u32> resourceLifetime{ std::numeric_limits<u32>::max(), std::numeric_limits<u32>::min() };		// Lifetime of the underlying resource
			D3D12_RESOURCE_STATES currState{ D3D12_RESOURCE_STATE_COMMON };
		};

		struct RGResourceImported
		{
			D3D12_RESOURCE_STATES importEntryState{ D3D12_RESOURCE_STATE_COMMON };
			D3D12_RESOURCE_STATES importExitState{ D3D12_RESOURCE_STATE_COMMON };
			D3D12_RESOURCE_STATES currState{ D3D12_RESOURCE_STATE_COMMON };
			
			// Infinite lifetime from graphs perspective
			std::pair<u32, u32> resourceLifetime{ std::numeric_limits<u32>::max(), std::numeric_limits<u32>::min() };
		};

		struct RGResourceAliased
		{
			RGResourceID prevID;
			RGResourceID originalID;		// Original Declared/Imported resource
		};

		struct RGResource
		{
			RGResourceType resourceType{ RGResourceType::Texture };
			RGResourceVariant variantType{ RGResourceVariant::Declared };
			std::variant<RGResourceDeclared, RGResourceImported, RGResourceAliased> variants;

			std::pair<u32, u32> usageLifetime{ std::numeric_limits<u32>::max(), std::numeric_limits<u32>::min() };		// Lifetime of this specific RGResource

			u64 resource{ 0 };	// texture/buffer
			bool hasBeenAliased{ false };

		};

		// ======================

	private:
		// RealizeResources be called after resource lifetimes have been resolved
		void RealizeResources();
		void SanitizeAliasingLifetimes();
		void ImportedResourceExitTransition(CommandList cmdl);
		void DeclaredResourceTransitionToInit(CommandList cmdl);

		void ResolveLifetime(RGResourceID id, u32 depth);

		u64 GetResource(RGResourceID id);
		RGResourceType GetResourceType(RGResourceID id) const;
		RGResourceVariant GetResourceVariant(RGResourceID id) const;
		D3D12_RESOURCE_STATES GetCurrentState(RGResourceID id) const;
		std::pair<u32, u32>& GetMutableUsageLifetime(RGResourceID id);		// Lifetime of specific graph resource

		// ========= Declared & Alias specific
		// Lifetime of underlying resource
		std::pair<u32, u32>& GetMutableResourceLifetime(RGResourceID id);
		const std::pair<u32, u32>& GetResourceLifetime(RGResourceID id);

		// Helper for JIT state transitions
		void SetCurrentState(RGResourceID id, D3D12_RESOURCE_STATES state);

		// Aliasing helper
		void SetTexture(RGResourceID id, Texture texture);

	private:
		RenderDevice* m_rd{ nullptr };
		GPUGarbageBin* m_bin{ nullptr };
		std::unordered_map<RGResourceID, RGResource> m_resources;

		// Structure for transfering imported resources on resource clear
		std::vector<std::pair<RGResourceID, RGResource>> m_importedTransfer;

		//MemoryPool m_memoryPool;

		MemoryPool m_rtDsTextureMemPool;
		MemoryPool m_nonRtDsTextureMemPool;
		MemoryPool m_bufferMemPool;

		GPUPoolMemoryInfo m_totalMemUsage;

		std::unordered_map<u32, std::vector<GPUBarrier>> m_aliasingBarrierPerDepLevel;
		std::vector<GPUBarrier> m_aliasingBarrierWrap;

	};
}