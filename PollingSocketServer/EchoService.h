#pragma once

#include <rapidjson\document.h>

class PollingSocket;
class EchoService
{
public:
	static void Init();
	static void Shutdown();

	static void OnRecv(PollingSocket* client, rapidjson::Document& data);
};
