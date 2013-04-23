#include "EchoService.h"

#include <string>

#include "Server.h"
#include "Log.h"

#include <rapidjson/document.h>

/*static*/ void EchoService::Init()
{
	LOG("EchoService::Init()");
}

/*static*/ void EchoService::Shutdown()
{
	LOG("EchoService::Shutdown()");
}

/*static*/ void EchoService::OnRecv(PollingSocket* client, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "echo")
	{
		client->AsyncSend(data);
	}
}
