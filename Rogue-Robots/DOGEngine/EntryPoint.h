#pragma once
#include "src/Core/Application.h"



int main(int, char**)
{
	DOG::Server serverTest;
	DOG::Client clientTest;
	serverTest.StartTcpServer();
	clientTest.ConnectTcpServer("192.168.50.214");

		//HR hr = CoInitialize(nullptr);
		//hr.try_fail("Failed to initialize COM");

		//std::unique_ptr<DOG::Application> app{ nullptr };
		//while (DOG::ApplicationManager::ShouldRestartApplication())
		//{
		//	if (!DOG::ApplicationManager::ShouldRestartApplication())
		//		app = std::move(CreateApplication());
		//	else
		//		app->OnRestart();

		//	app->Run();
		//}

		//CoUninitialize();
		while (true) {
			Sleep(1000);
			if (GetAsyncKeyState(87))
			{
				testInput.w = true;
				
			}
			testOutput = clientTest.SendandReciveTcp(testInput);
			std::cout << testOutput[0].w << std::endl;
			testInput.w = false;

			continue;
		}
		return 0;
	}

	extern std::unique_ptr<DOG::Application> CreateApplication() noexcept;
