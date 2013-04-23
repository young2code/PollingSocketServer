#pragma once

#include "TSingleton.h"
#include "PollingSocket.h"
#include <vector>
#include <rapidjson\document.h>

class Server :  public TSingleton<Server>
{
public:
	Server(void);
	virtual ~Server(void);

public:
	bool Init(unsigned short port);
	void Shutdown();

	void Update();

private:
	void OnAccept(PollingSocket* listenSocket);
	void OnRecv(PollingSocket* socket, bool parsingError, rapidjson::Document& data);
	void OnClose(PollingSocket* socket);
	void OnConnect(PollingSocket* socket); // dummy

private:
	PollingSocket mListenSocket;
	std::vector<PollingSocket*> mClientSockets;	
};

