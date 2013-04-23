#include <string>
#include <iostream>
using namespace std;

#include "Log.h"
#include "Network.h"
#include "Server.h"

void main(int argc, char* argv[])
{
	Log::Init();

	if (argc != 2)
	{
		LOG("Please add port number");
		LOG("(ex) 17000");
		return;
	}

	u_short port = static_cast<u_short>( atoi(argv[1]) );

	LOG("Input : port : %d", port);

	if(Network::Init() == false)
	{
		ERROR_MSG("Network::Init() failed");
		return;
	}

	Server::Create();
	
	if(Server::Instance()->Init(port) == false)
	{
		ERROR_MSG("Server::Init() failed");
		return;
	}

#ifndef _DEBUG
	Log::EnableTrace(false);
#endif

	bool loop = true;
	while(loop)
	{
		Server::Instance()->Update();
	}

	Server::Instance()->Shutdown();

	Server::Destroy();

	Network::Shutdown();

	Log::Shutdown();

	return;
}
