#include "Server.h"


Server::Server()
{
	m_gameAlive = false;
	m_playerIds.resize(MAX_PLAYER_COUNT);
	m_outputUdp.nrOfEntites = 0;
	m_outputUdp.udpId = 0;
	for (UINT8 i = 0; i < MAX_PLAYER_COUNT; i++)
	{
		m_playersServer[i].playerId = i;
		m_playersServer[i].matrix = {};
	
		m_holdPlayersUdp[i].playerId = i;
		m_holdPlayersUdp[i].matrix = {};
		m_holdPlayersUdp[i].jump  = false;
		m_holdPlayersUdp[i].shoot  = false;
		m_holdPlayersUdp[i].activateActiveItem  = false;

		
		m_playerIds.at(i) = i;

	}

	//Change denominator to set tick rate
	m_tickrateTcp = 1.0f / 60.0f;
	m_tickrateUdp = 1.0f / 60.0f;
	m_upid = 0;
	m_reciveupid = 0;
	m_clientsSocketsTcp.clear();
}

Server::~Server()
{
	if (m_gameAlive)
	{
		m_gameAlive = FALSE;
		m_loopTcp.join();
		m_reciveConnectionsTcp.join();
		m_loopUdp.join();
		m_reciveLoopUdp.join();
	}
	for (UINT8 socketIndex = 0; socketIndex < m_holdPlayerIds.size(); socketIndex++)
	{
		std::cout << "Server: Closes socket for player" << m_holdPlayerIds.at(socketIndex) + 1 << std::endl;
		m_playerIds.push_back(m_holdPlayerIds.at(socketIndex));
		m_holdPlayerIds.erase(m_holdPlayerIds.begin() + socketIndex);
		m_clientsSocketsTcp.erase(m_clientsSocketsTcp.begin() + socketIndex);
	}
	WSACleanup();
}

bool Server::StartTcpServer()
{
	std::cout << "Server: Starting server..." << std::endl;

	int check;
	unsigned long setUnblocking = 1;
	WSADATA socketStart;
	SOCKET listenSocket = INVALID_SOCKET;
	addrinfo* addrOutput = NULL, addrInput;

	ZeroMemory(&addrInput, sizeof(addrInput));
	addrInput.ai_family = AF_INET;
	addrInput.ai_socktype = SOCK_STREAM;
	addrInput.ai_protocol = IPPROTO_TCP;
	addrInput.ai_flags = AI_PASSIVE;

	check = WSAStartup(0x202, &socketStart);
	if (check != 0)
	{
		std::cout << "Server: Failed to start WSA on server, ErrorCode: " << check << std::endl;
		return FALSE;
	}

	check = getaddrinfo(NULL, PORTNUMBER_OUT, &addrInput, &addrOutput);
	if (check != 0)
	{
		std::cout << "Server: Failed to getaddrinfo on server, ErrorCode: " << check << std::endl;
		return FALSE;
	}

	//Create socket that listens for new connections
	listenSocket = socket(addrOutput->ai_family, addrOutput->ai_socktype, addrOutput->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "Server: Failed to create listensocket on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return FALSE;
	}


	check = ioctlsocket(listenSocket, FIONBIO, (unsigned long*)&setUnblocking);
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to set listensocket to unblocking on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return FALSE;
	}

	check = bind(listenSocket, addrOutput->ai_addr, (int)addrOutput->ai_addrlen);
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to bind listenSocket on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return FALSE;
	}

	check = listen(listenSocket, SOMAXCONN);
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to SOMAXCONN on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return FALSE;
	}

	m_gameAlive = TRUE;
	//Thread to handle new connections
	m_reciveConnectionsTcp = std::thread(&Server::ServerReciveConnectionsTCP, this, listenSocket);

	//Thread that runs Game Tcp
	m_loopTcp = std::thread(&Server::ServerPollTCP, this);

	//Thread that runs Game Udp
	m_loopUdp = std::thread(&Server::GameLoopUdp, this);

	//Tread that parses udp packets
	m_reciveLoopUdp = std::thread(&Server::ReciveLoopUdp, this);

	std::cout << "Server: Server started" << std::endl;

	return TRUE;
}

void Server::ServerReciveConnectionsTCP(SOCKET listenSocket)
{
	char inputSend[sizeof(int)];
	while (m_gameAlive)
	{
		SOCKET clientSocket = accept(listenSocket, NULL, NULL);
		//Check if server full
		if (clientSocket != INVALID_SOCKET)
		{
			if (m_playerIds.empty())
			{
				sprintf_s(inputSend, sizeof(int), "%d", -1);
				send(clientSocket, inputSend, sizeof(int), 0);
			}
			else
			{
				{
					bool turn = true;
					std::cout << "Server: Connection Accepted" << std::endl;
					setsockopt(clientSocket, SOL_SOCKET, TCP_NODELAY, (char*)&turn, sizeof(bool));
					WSAPOLLFD m_clientPoll;
					Client::ClientsData input;
					std::cout << "\nServer: Accept a connection from clientSocket: " << clientSocket << ", From player: " << m_playerIds.front() + 1 << std::endl;
					//give connections a player id
					UINT8 playerId = m_playerIds.front();
					input.playerId = playerId;
					m_holdPlayerIds.push_back(playerId);
					sprintf_s(inputSend, sizeof(int), "%d", playerId);
					send(clientSocket, inputSend, sizeof(int), 0);
					m_playerIds.erase(m_playerIds.begin());

					//store client socket
					m_clientPoll.fd = clientSocket;
					m_clientPoll.events = POLLRDNORM;
					m_clientPoll.revents = 0;
					m_clientsSocketsTcp.push_back(m_clientPoll);
				}
			}
		}
	}
	closesocket(listenSocket);
}


float Server::TickTimeLeftTCP(LARGE_INTEGER t, LARGE_INTEGER frequency)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return float(now.QuadPart - t.QuadPart) / float(frequency.QuadPart);
}

void Server::ServerPollTCP()
{
	std::cout << "Server: Started to tick" << std::endl;

	LARGE_INTEGER tickStartTime;
	Client::ClientsData holdClientsData;
	LARGE_INTEGER clockFrequency;
	QueryPerformanceFrequency(&clockFrequency);
	
	//sets the minium resolution for ticks
	UINT sleepGranularityMs = 1;
	timeBeginPeriod(sleepGranularityMs);
	

	do {
		QueryPerformanceCounter(&tickStartTime);
		char sendBuffer[SEND_AND_RECIVE_BUFFER_SIZE];
		char reciveBuffer[SEND_AND_RECIVE_BUFFER_SIZE];
		int bufferSendSize = 0;
		int bufferReciveSize = 0;
		m_holdSocketsTcp = m_clientsSocketsTcp;
		bufferSendSize += sizeof(m_playersServer);
		if (WSAPoll(m_holdSocketsTcp.data(), (u32)m_holdSocketsTcp.size(), 1) > 0)
		{
			for (int i = 0; i < m_holdSocketsTcp.size(); ++i)
			{

				if (m_holdSocketsTcp[i].revents & POLLERR || m_holdSocketsTcp[i].revents & POLLHUP || m_holdSocketsTcp[i].revents & POLLNVAL)
					CloseSocketTCP(i);
				//TODO TRY HERE
				//read in from clients that have send data
				else if (m_holdSocketsTcp[i].revents & POLLRDNORM)
				{
					bufferReciveSize = 0;
					recv(m_holdSocketsTcp[i].fd, reciveBuffer, SEND_AND_RECIVE_BUFFER_SIZE, 0);
					memcpy(&holdClientsData, &reciveBuffer, sizeof(Client::ClientsData));
					memcpy(&m_playersServer[holdClientsData.playerId], &holdClientsData, sizeof(Client::ClientsData));
					bufferReciveSize += sizeof(Client::ClientsData);

					//check if host and then add transforms
					if (holdClientsData.playerId == 0)
					{
						for (int j = 0; j < holdClientsData.nrOfNetTransform; j++)
						{
							DOG::NetworkTransform test;
							memcpy(sendBuffer + bufferSendSize, reciveBuffer + bufferReciveSize, sizeof(DOG::NetworkTransform));
							bufferReciveSize += sizeof(DOG::NetworkTransform);
							bufferSendSize += sizeof(DOG::NetworkTransform);
						}
					}

				}
			}

		}

		
		memcpy(sendBuffer, (char*)m_playersServer, sizeof(Client::ClientsData) * MAX_PLAYER_COUNT);
		//memcpy(reciveBuffer, (char*)&m_playersServer, sizeof(Client::ClientsData) *4);
		for (int i = 0; i < m_holdSocketsTcp.size(); ++i)
			send(m_holdSocketsTcp[i].fd, sendBuffer, bufferSendSize, 0);

		//wait untill tick is done 
		float timeTakenS = TickTimeLeftTCP(tickStartTime, clockFrequency);

		while (timeTakenS < m_tickrateTcp)
		{
			float timeToWaitMs = (m_tickrateTcp - timeTakenS) * 1000;

			if (timeToWaitMs > 0)
			{
				Sleep((u32)timeToWaitMs);
			}
			timeTakenS = TickTimeLeftTCP(tickStartTime, clockFrequency);
		}
	} while (m_gameAlive);
	std::cout << "Server: server loop closed" << std::endl;
}

void Server::CloseSocketTCP(int socketIndex)
{
	std::cout << "Server: Closes socket for player" << m_holdPlayerIds.at(socketIndex) + 1 << std::endl;
	m_playerIds.push_back(m_holdPlayerIds.at(socketIndex));
	m_holdPlayerIds.erase(m_holdPlayerIds.begin() + socketIndex);
	m_clientsSocketsTcp.erase(m_clientsSocketsTcp.begin() + socketIndex);
}

std::string Server::GetIpAddress()
{
	int check;
	char hold[32];
	std::string ip = "";
	struct addrinfo* result, * nextResult;

	check = gethostname(hold, sizeof(hold));
	if (check == SOCKET_ERROR)
	{
		std::cout << "GetIpAddress: gethostname failed, error code: " << WSAGetLastError() << std::endl;
		return ip;
	}
	check = getaddrinfo(hold, NULL, NULL, &result);
	if (check == SOCKET_ERROR)
	{
		std::cout << "GetIpAddress: getaddrinfo failed, error code: " << WSAGetLastError() << std::endl;
		return ip;
	}
	nextResult = result;
	while (nextResult)
	{
		if (nextResult->ai_family == AF_INET)
			inet_ntop(nextResult->ai_family, &((struct sockaddr_in*)nextResult->ai_addr)->sin_addr, hold, 31);
		nextResult = nextResult->ai_next;
	}
	for (int i = 0; i < sizeof(hold); i++)
	{
		if (hold[i] == '\0')
			break;
		ip.push_back(hold[i]);

	}
	freeaddrinfo(result);
	return ip;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Server::GameLoopUdp()
{
	// Send udp socket
	SOCKET udpSendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSendSocket == INVALID_SOCKET)
	{
		std::cout << "Server: Failed to create udpSocket on client, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}
	struct sockaddr_in clientAddressUdp;
	ZeroMemory(&clientAddressUdp, sizeof(clientAddressUdp));
	clientAddressUdp.sin_family = AF_INET;

	inet_pton(AF_INET, MULTICAST_ADRESS, &clientAddressUdp.sin_addr.s_addr);
	clientAddressUdp.sin_port = htons(PORTNUMBER_OUT_INT);

	LARGE_INTEGER tickStartTime;
	LARGE_INTEGER clockFrequency;
	QueryPerformanceFrequency(&clockFrequency);

	//sets the minium resolution for ticks
	UINT sleepGranularityMs = 1;
	timeBeginPeriod(sleepGranularityMs);
	Client::UdpData holdHeaderUdp;

	while (m_gameAlive)
	{
		QueryPerformanceCounter(&tickStartTime);
		m_outputUdp.udpId++;
		holdHeaderUdp = m_outputUdp;


		char sendBuffer[SEND_AND_RECIVE_BUFFER_SIZE];
		memcpy(sendBuffer, &holdHeaderUdp, sizeof(Client::UdpData));
		m_outputUdp.nrOfEntites = 0;

		m_mut.lock();
		memcpy(sendBuffer + sizeof(holdHeaderUdp), m_holdPlayersUdp, sizeof(m_holdPlayersUdp));
		m_mut.unlock();


		holdHeaderUdp.udpId = m_upid++;
		sendto(udpSendSocket, sendBuffer, sizeof(holdHeaderUdp) + sizeof(m_holdPlayersUdp), 0, (struct sockaddr*)&clientAddressUdp, sizeof(clientAddressUdp));

		//wait untill tick is done 
		float timeTakenS = TickTimeLeftTCP(tickStartTime, clockFrequency);

		while (timeTakenS < m_tickrateUdp)
		{
			float timeToWaitMs = (m_tickrateUdp - timeTakenS) * 1000;
			if (timeToWaitMs > 0)
			{
				Sleep((u32)timeToWaitMs);
			}
			timeTakenS = TickTimeLeftTCP(tickStartTime, clockFrequency);
		}
	}

	std::cout << "Server: udp loop closed" << std::endl;
}

void Server::ReciveLoopUdp()
{
	SOCKET udpSocket;
	int check;
	struct ip_mreq setMulticast;
	struct sockaddr_in hostAddress;
	bool turn = true;
	DWORD ttl = 1000;
	int hostAddressLength = sizeof(hostAddress);
	Client::PlayerNetworkComponent holderPlayer;

	udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSocket == INVALID_SOCKET)
	{
		std::cout << "Server: Failed to create udpSocket on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}

	check = setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&turn, sizeof(bool));
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to set udpsocket to reusabale adress to unblocking on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}

	ZeroMemory(&hostAddress, sizeof(hostAddress));
	hostAddress.sin_family = AF_INET;
	hostAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	hostAddress.sin_port = htons(PORTNUMBER_IN_INT);

	check = bind(udpSocket, (struct sockaddr*)&hostAddress, sizeof(hostAddress));
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to bind udpsocket on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}

	inet_pton(AF_INET, MULTICAST_ADRESS, &setMulticast.imr_multiaddr.S_un.S_addr);
	setMulticast.imr_interface.S_un.S_addr = htonl(INADDR_ANY);
	check = setsockopt(udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&setMulticast, sizeof(setMulticast));
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to set assign multicast on udp on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}

	check = setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ttl, sizeof(ttl));
	if (check == SOCKET_ERROR)
	{
		std::cout << "Server: Failed to set socket to ttl on server, ErrorCode: " << WSAGetLastError() << std::endl;
		return;
	}

	int bytesRecived = 0;

	//Gameloop
	while (m_gameAlive)
	{
		bytesRecived = recvfrom(udpSocket, (char*)&holderPlayer, SEND_AND_RECIVE_BUFFER_SIZE, 0, (struct sockaddr*)&hostAddress, &hostAddressLength);
		if (bytesRecived > -1)
		{
			if (holderPlayer.udpId > m_holdPlayersUdp[holderPlayer.playerId].udpId)
			{
				m_mut.lock();
				memcpy(&m_holdPlayersUdp[holderPlayer.playerId], &holderPlayer, sizeof(holderPlayer));
				m_mut.unlock();
			}
		}
	}
}