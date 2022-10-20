#pragma once
#include "../ECS/Component.h"

namespace DOG
{
	namespace gfx
	{
		class Renderer;
		class MaterialTable;
	}

	class CustomMaterialManager
	{
	public:
		static void Initialize(gfx::Renderer* renderer);
		static void Destroy();
		static CustomMaterialManager& Get();

		MaterialHandle AddMaterial(const MaterialDesc& desc);
		void UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc);
		void RemoveMaterial(MaterialHandle handle);

	private:
		CustomMaterialManager(gfx::Renderer* renderer);
		~CustomMaterialManager() = default;

	private:
		static CustomMaterialManager* s_instance;
		gfx::MaterialTable* m_materialTable;


	};
}