#include "Server.h"

namespace DOG
{
	Server::Server()
	{
		m_playerIds.resize(m_nrOfPlayers);
		for (int i = 0; i < m_nrOfPlayers; i++)
		{
			m_playersServer[i].player_nr = i + 1;
			m_playerIds.at(i) = i;
		}

		//Change denominator to set tick rate
		m_tickrate = 1.0f / 2.0f;


		m_clientsSockets.clear();
	}

	Server::~Server()
	{

	}

	void Server::StartTcpServer()
	{
		std::cout << "Server: Starting server..." << std::endl;
		int check;
		WSADATA socketStart;
		SOCKET listenSocket = INVALID_SOCKET;
		addrinfo* addrOutput = NULL, addrInput;

		ZeroMemory(&addrInput, sizeof(addrInput));
		addrInput.ai_family = AF_INET;
		addrInput.ai_socktype = SOCK_STREAM;
		addrInput.ai_protocol = IPPROTO_TCP;
		addrInput.ai_flags = AI_PASSIVE;

		check = WSAStartup(0x202, &socketStart);
		assert(check == 0);

		check = getaddrinfo(NULL, "50005", &addrInput, &addrOutput);
		assert(check == 0);

		//Create socket that listens for new connections
		listenSocket = socket(addrOutput->ai_family, addrOutput->ai_socktype, addrOutput->ai_protocol);
		assert(listenSocket != INVALID_SOCKET);

		check = bind(listenSocket, addrOutput->ai_addr, (int)addrOutput->ai_addrlen);
		assert(check != SOCKET_ERROR);

		check = listen(listenSocket, SOMAXCONN);
		assert(check != SOCKET_ERROR);

		//Create a new thread for each connecting client
		std::thread test = std::thread(&Server::ServerReciveConnections, this, listenSocket);
		test.detach();
		std::thread test2 = std::thread(&Server::ServerPoll, this);
		test2.detach();
		std::cout << "Server: Server started" << std::endl;

	}

	void Server::ServerReciveConnections(SOCKET listenSocket)
	{


		char* inputSend = new char[sizeof(int)];
		while (true)
		{
			std::cout << "Server: Waiting for connections: " << std::endl;
			SOCKET clientSocket = accept(listenSocket, NULL, NULL);
			//Check if server full
			if (m_playerIds.empty())
			{
				sprintf(inputSend, "%d", -1);
				send(clientSocket, inputSend, sizeof(int), 0);
			}
			else
			{
				{
					std::cout << "Server: Connection Accepted" << std::endl;

					
					WSAPOLLFD m_clientPoll;
					std::cout << "\nServer: Accept a connection from clientSocket: " << clientSocket << ", From player: " << m_playerIds.front() + 1 << std::endl;
					int playerId = m_playerIds.front();
					sprintf(inputSend, "%d", playerId);
					send(clientSocket, inputSend, sizeof(int), 0);
					
					m_clientPoll.fd = clientSocket;
					m_clientPoll.events = POLLRDNORM;
					m_clientPoll.revents = 0;
					m_clientsSockets.push_back(m_clientPoll);

					m_playerIds.erase(m_playerIds.begin());


				}
			}
		}

		closesocket(listenSocket);
	}


	float Server::TickTimeLeft(LARGE_INTEGER t, LARGE_INTEGER frequency)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		return float(now.QuadPart - t.QuadPart) / float(frequency.QuadPart);
	}

	void Server::ServerPoll()
	{
		std::cout << "Server: connected to ServerSend" << std::endl;
		BOOL turn = true;



		LARGE_INTEGER tickStartTime;
		LARGE_INTEGER timeNow;
		SOCKET clientSocket;
		const UINT sleepGranularityMs = 1;
		ClientsData holdClientsData;
		char* clientData = new char[sizeof(ClientsData)];
		char* inputSend = new char[sizeof(m_playersServer)];

		int status = 0;


		LARGE_INTEGER clockFrequency;
		QueryPerformanceFrequency(&clockFrequency);


		do {
			QueryPerformanceCounter(&tickStartTime);

			m_holdSockets = m_clientsSockets;
			if (WSAPoll(m_holdSockets.data(), m_holdSockets.size(), 1) > 0)
			{
				for (int i = 0; i < m_holdSockets.size(); i++)
				{
					if (m_holdSockets[i].revents & POLLERR || m_holdSockets[i].revents & POLLHUP || m_holdSockets[i].revents & POLLNVAL) 
						CloseSocket(i);

					else if (m_holdSockets[i].revents & POLLRDNORM)
					{
						status = recv(m_holdSockets[i].fd, clientData, sizeof(m_playersServer), 0);
						memcpy(&holdClientsData, (void*)clientData, sizeof(ClientsData));
						memcpy(&m_playersServer[holdClientsData.player_nr-1], (void*)clientData, sizeof(ClientsData));
					}
				}
			}
			memcpy(inputSend, m_playersServer, sizeof(m_playersServer));

			for (int i = 0; i < m_holdSockets.size(); i++)
			{
				status = send(m_holdSockets[i].fd, inputSend, sizeof(m_playersServer), 0);
			}
			//wait untill tick is done 
			float timeTakenS = TickTimeLeft(tickStartTime, clockFrequency);

			while (timeTakenS < m_tickrate)
			{
				float timeToWaitMs = (m_tickrate - timeTakenS) * 1000;

				if (timeToWaitMs > 0)
				{

					Sleep(timeToWaitMs);
				}
				timeTakenS = TickTimeLeft(tickStartTime, clockFrequency);
			}
		} while (true);
		std::cout << "Server: server thread closes..." << std::endl;
		
		if (closesocket(clientSocket) == SOCKET_ERROR)
			std::cout << "Server: AAaAAA N�t �r j�vligt fel" << WSAGetLastError() << std::endl;
	}

	void Server::CloseSocket(int socketIndex)
	{
		std::cout << "Server: Closes socket" << std::endl;
		//m_playerIds.push_back(playerIndex);
		m_clientsSockets.erase(m_clientsSockets.begin() +socketIndex);
	}
}
