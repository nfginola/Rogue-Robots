#include "GameLayer.h"
#include "Scripting/LuaGlobal.h"
#include "Scripting/ScriptManager.h"

using namespace DOG;
using namespace DirectX;
using namespace DirectX::SimpleMath;
GameLayer::GameLayer() noexcept
	: Layer("Game layer"), m_entityManager{ DOG::EntityManager::Get() }
{
	auto& am = DOG::AssetManager::Get();
	m_redCube = am.LoadModelAsset("Assets/red_cube.glb");
	m_greenCube = am.LoadModelAsset("Assets/green_cube.glb");
	m_blueCube = am.LoadModelAsset("Assets/blue_cube.glb");
	m_magentaCube = am.LoadModelAsset("Assets/magenta_cube.glb");

	entity entity1 = m_entityManager.CreateEntity();
	m_entityManager.AddComponent<ModelComponent>(entity1, m_redCube);
	m_entityManager.AddComponent<TransformComponent>(entity1)
		.SetPosition({ 4, -2, 5 })
		.SetRotation({ 3.14f / 4.0f, 0, 0 })
		.SetScale({0.5, 0.5, 0.5});

	entity entity2 = m_entityManager.CreateEntity();
	m_entityManager.AddComponent<ModelComponent>(entity2, m_greenCube);
	m_entityManager.AddComponent<TransformComponent>(entity2, Vector3(-4, -2, 5), Vector3(0.1f, 0, 0));

	entity entity3 = m_entityManager.CreateEntity();
	m_entityManager.AddComponent<ModelComponent>(entity3, m_blueCube);
	auto& t3 = m_entityManager.AddComponent<TransformComponent>(entity3);
	t3.SetPosition({ 4, 2, 5 });
	t3.SetScale({ 0.5f, 0.5f, 0.5f });

	entity entity4 = m_entityManager.CreateEntity();
	m_entityManager.AddComponent<ModelComponent>(entity4, m_magentaCube);
	auto& t4 = m_entityManager.AddComponent<TransformComponent>(entity4);
	t4.worldMatrix(3, 0) = -4;
	t4.worldMatrix(3, 1) = 2;
	t4.worldMatrix(3, 2) = 5;
}

void GameLayer::OnAttach()
{
	//Register Lua interfaces
	RegisterLuaInterfaces();
	//...
}

void GameLayer::OnDetach()
{

}

void GameLayer::OnUpdate()
{
	m_player.OnUpdate();
}

void GameLayer::OnRender()
{
	//...
}

void GameLayer::OnImGuiRender()
{
	//...
}

//Place-holder example on how to use event system:
void GameLayer::OnEvent(DOG::IEvent& event)
{
	using namespace DOG;
	switch (event.GetEventType())
	{
	case EventType::LeftMouseButtonPressedEvent:
	{
		//auto [x, y] = EVENT(LeftMouseButtonPressedEvent).coordinates;
		//std::cout << GetName() << " received event: Left MB clicked [x,y] = [" << x << "," << y << "]\n";
		break;
	}
	}
}

void GameLayer::RegisterLuaInterfaces()
{
	LuaGlobal global(&LuaW::s_luaW);

	//-----------------------------------------------------------------------------------------------
	//Input
	//Create a luaInterface variable that holds the interface object (is reused for all interfaces)
	std::shared_ptr<LuaInterface> luaInterfaceObject = std::make_shared<InputInterface>();
	m_luaInterfaces.push_back(luaInterfaceObject); //Add it to the gamelayer's interfaces.
	
	auto luaInterface = global.CreateLuaInterface("InputInterface"); //Register a new interface in lua.
	//Add all functions that are needed from the interface class.
	luaInterface.AddFunction<InputInterface, &InputInterface::IsLeftPressed>("IsLeftPressed");
	luaInterface.AddFunction<InputInterface, &InputInterface::IsRightPressed>("IsRightPressed");
	luaInterface.AddFunction<InputInterface, &InputInterface::IsKeyPressed>("IsKeyPressed");
	global.SetLuaInterface(luaInterface);
	//Make the object accessible from lua. Is used by: Input.FunctionName()
	global.SetUserData<LuaInterface>(luaInterfaceObject.get(), "Input", "InputInterface");

	//-----------------------------------------------------------------------------------------------
	//Audio
	luaInterfaceObject = std::make_shared<AudioInterface>();
	m_luaInterfaces.push_back(luaInterfaceObject);

	luaInterface = global.CreateLuaInterface("AudioInterface");
	//luaInterface.AddFunction<AudioInterface, &InputInterface::PlaySound>("PlaySound");
	global.SetLuaInterface(luaInterface);

	global.SetUserData<LuaInterface>(luaInterfaceObject.get(), "Audio", "AudioInterface");


}