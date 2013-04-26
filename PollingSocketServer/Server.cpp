#include "Server.h"
#include <boost/bind.hpp>

#include "Network.h"
#include "Log.h"

#include "EchoService.h"
#include "TicTacToeService.h"
#include "CheckerService.h"

Server::Server(void)
{
}


Server::~Server(void)
{
}


bool Server::Init(unsigned short port)
{
	LOG("Server::Init() - port[%d]", port);

	EchoService::Init();
	TicTacToeService::Init();
	CheckerService::Init();

	PollingSocket::OnAcceptFunc onAccept = boost::bind(&Server::OnAccept, this, _1);
	PollingSocket::OnCloseFunc onClose = boost::bind(&Server::OnClose, this, _1);

	return mListenSocket.InitListen(port, onAccept, onClose);
}


void Server::Shutdown()
{
	LOG("Server::Shutdown()");

	mListenSocket.Shutdown(false);

	for (size_t i = 0 ; i < mClientSockets.size() ; ++i)
	{
		mClientSockets[i]->Shutdown();
		delete mClientSockets[i];
	}
	mClientSockets.clear();

	CheckerService::Shutdown();
	TicTacToeService::Shutdown();
	EchoService::Shutdown();
}


void Server::Update()
{
	mListenSocket.Poll();

	for (size_t i = 0 ; i < mClientSockets.size() ; ++i)
	{
		mClientSockets[i]->Poll();
	}

	TicTacToeService::Update();
	CheckerService::Update();
}


void Server::OnAccept(PollingSocket* listenSocket)
{
	SOCKET socket = accept(mListenSocket.GetSocket(), NULL, NULL);

    if (socket == INVALID_SOCKET) 
	{
		assert(WSAEWOULDBLOCK != WSAGetLastError());
		ERROR_CODE(WSAGetLastError(), "Server::OnAccept() - failed");
		mListenSocket.Shutdown();
		return;
	}

	PollingSocket* newClient = new PollingSocket;
	PollingSocket::OnRecvFunc onRecv = boost::bind(&Server::OnRecv, this, _1, _2, _3);
	PollingSocket::OnCloseFunc onClose = boost::bind(&Server::OnClose, this, _1);

	newClient->InitAccept(socket, onRecv, onClose);

	mClientSockets.push_back(newClient);

	std::string address;
	unsigned short port;
	Network::GetRemoteAddress(socket, address, port);
	LOG("Server::OnAccept() - client [%s:%d]", address.c_str(), port);
}


void Server::OnRecv(PollingSocket* socket, bool parsingError, rapidjson::Document& data)
{
	EchoService::OnRecv(socket, data);
	TicTacToeService::OnRecv(socket, data);
	CheckerService::OnRecv(socket, data);
}


void Server::OnClose(PollingSocket* socket)
{
	auto itor = std::find(mClientSockets.begin(), mClientSockets.end(), socket);
	if (itor != mClientSockets.end())
	{
		TicTacToeService::RemoveClient(*itor);
		CheckerService::RemoveClient(*itor);

		delete *itor;
		mClientSockets.erase(itor);
	}
}

