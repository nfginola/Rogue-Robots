#pragma once
#include <DOGEngine.h>
#include "..\Game\EntitesTypes.h"
#include  "..\Network\Client.h"
#include  "..\Network\Server.h"
#include "..\Game\MainPlayer.h"
#include "AgentManager/AgentManager.h"

enum PlayerActions
{
	Shoot = 0x01,
	Jump = 0x02,
	Temp1 = 0x04,
	Temp2 = 0x08,
	Temp3 = 0x10,
	Temp4 = 0x20
};
class NetCode
{
public:
	NetCode();
	~NetCode();
	
	void OnUpdate(AgentManager* agentManager);
	void OnStartup();
	bool Host();
	bool Join(char* input);
	INT8 Play();
	u8 GetNrOfPlayers();
	std::string GetIpAdress();
	bool IsLobbyAlive();
private:
	void Receive();
	void ReceiveUdp();
	
	void UpdateSendUdp();
	void AddMatrixUdp(DirectX::XMMATRIX input);

	
	ClientsData m_inputTcp;
	PlayerNetworkComponentUdp m_playerInputUdp;


	UdpReturnData m_outputUdp;
	
	std::atomic_bool m_active;
	std::atomic_bool m_startUp;
	std::atomic_bool m_hardSyncTcp;
	uint16_t m_tick = 0;
	std::atomic_bool m_netCodeAlive;
	std::thread m_thread;
	std::thread m_threadUdp;
	std::vector<DOG::entity> m_playersId;
	std::string m_inputString;
	Client m_client;
	std::mutex m_mut;
	u16 m_bufferSize;
	int m_bufferReceiveSize;
	char m_sendBuffer[SEND_AND_RECIVE_BUFFER_SIZE];
	char* m_receiveBuffer;
	bool m_dataIsReadyToBeReceivedTcp;
	bool m_lobby;
	Server m_serverHost;
	int m_numberOfPackets;
};