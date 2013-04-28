#include "SnakeCyclesService.h"

#include <algorithm>

#include "Server.h"
#include "Log.h"

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <boost/bind.hpp>

namespace
{
	const int kInitCountdown = 5;
}

/*static*/ SnakeCyclesService::ServiceList SnakeCyclesService::sServices;

/*static*/ void SnakeCyclesService::Init()
{
	LOG("SnakeCyclesService::Init()");
}

/*static*/ void SnakeCyclesService::Shutdown()
{
	LOG("SnakeCyclesService::Shutdown()");

	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		delete sServices[i];
	}
	sServices.clear();
}


/*static*/ void SnakeCyclesService::Update()
{
	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		sServices[i]->UpdateInternal();
	}

	Flush();
}

/*static*/ void SnakeCyclesService::OnRecv(PollingSocket* client, rapidjson::Document& data)
{
	CreateOrEnter(client, data);

	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		sServices[i]->OnRecvInternal(client, data);
	}
}

/*static*/ void SnakeCyclesService::RemoveClient(PollingSocket* client)
{
	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		if (!sServices[i]->RemoveClientInternal(client))
		{
			return;
		}
	}
}

/*static*/ void SnakeCyclesService::CreateOrEnter(PollingSocket* client, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "service_create")
	{
		assert(data["name"].IsString());
		std::string name(data["name"].GetString());

		if (name == "snakecycles")
		{
			for (size_t i = 0 ; i < sServices.size() ; ++i)
			{
				SnakeCyclesService* service = sServices[i];

				if (service->mFSM.GetState() == kStateWait || service->mFSM.GetState() == kStateCountdown)
				{
					if (service->mPlayers.size() < kMaxPlayers)
					{
						service->AddClient(client);
						return;
					}
				}
			}

			SnakeCyclesService* newService = new SnakeCyclesService;
			newService->AddClient(client);
			sServices.push_back(newService);
		}
	}
}

/*static*/ void SnakeCyclesService::Flush()
{
	for (auto itor = sServices.begin() ; itor != sServices.end() ; )
	{
		SnakeCyclesService* service = *itor;

		if (service->mFSM.GetState() == kStateWait && service->mPlayers.empty())
		{
			delete service;
			itor = sServices.erase(itor);
		}
		else
		{
			++itor;
		}
	}
}

SnakeCyclesService::SnakeCyclesService(void)
	: mCoutdownRemaing(0)
	, mCountdownSent(0)
	, mWinner(kPlayerMax)
{
	InitFSM();
}


SnakeCyclesService::~SnakeCyclesService(void)
{
	ShutdownFSM();
}

void SnakeCyclesService::InitFSM()
{
#define BIND_CALLBACKS(State) boost::bind(&SnakeCyclesService::OnEnter##State, this, _1), \
							  boost::bind(&SnakeCyclesService::OnUpdate##State, this, _1), \
							  boost::bind(&SnakeCyclesService::OnLeave##State, this, _1)

	mFSM.RegisterState(kStateWait, BIND_CALLBACKS(Wait));
	mFSM.RegisterState(kStateCountdown, BIND_CALLBACKS(Countdown));
	mFSM.RegisterState(kStatePlay, BIND_CALLBACKS(Play));
	mFSM.RegisterState(kStateEnd, BIND_CALLBACKS(End));

#undef BIND_CALLBACKS

	mFSM.SetState(kStateWait);
}

void SnakeCyclesService::ShutdownFSM()
{
	mFSM.Reset(false);
}


void SnakeCyclesService::UpdateInternal()
{
	CheckPlayerConnection();

	mFSM.Update();
}


void SnakeCyclesService::OnRecvInternal(PollingSocket* client, rapidjson::Document& data)
{
	auto itor = std::find_if(mPlayers.begin(), mPlayers.end(), [client](const Player& player){ return player.GetClient() == client; } );

	if (itor == mPlayers.end())
	{
		return;
	}

	switch(mFSM.GetState())
	{
	case kStateWait:		OnRecvWait(*itor, data);		break;
	case kStateCountdown:	OnRecvCountdown(*itor, data);	break;
	case kStatePlay:		OnRecvPlay(*itor, data);		break;
	case kStateEnd:			OnRecvEnd(*itor, data);			break;

	default:
		assert(0);
		return;
	}
}


void SnakeCyclesService::AddClient(PollingSocket* client)
{
	assert(mFSM.GetState() == kStateWait || mFSM.GetState() == kStateCountdown);
	assert(mPlayers.size() < kMaxPlayers);

	Player newPlayer(client);
	mPlayers.push_back(newPlayer);
}


bool SnakeCyclesService::RemoveClientInternal(PollingSocket* client)
{
	auto itor = std::find_if(mPlayers.begin(), mPlayers.end(), [client](const Player& player){ return player.GetClient() == client; } );
	if (itor != mPlayers.end())
	{
		if (mWinner == itor->GetIndex())
		{
			LOG("SnakeCyclesService::RemoveClientInternal() - winner[%d] left the game.", mWinner);
			mWinner = kPlayerMax;
		}
		mPlayers.erase(itor);
		return true;
	}
	return false;
}


void SnakeCyclesService::CheckPlayerConnection()
{
	if (mFSM.GetState() == kStateCountdown || mFSM.GetState() == kStatePlay)
	{
		if (mPlayers.size() < kMinPlayers)
		{
			mFSM.SetState(kStateEnd);
		}
	}
}


void SnakeCyclesService::Send(PollingSocket* client, rapidjson::Document& data) const
{
	client->AsyncSend(data);
}


void SnakeCyclesService::Broadcast(rapidjson::Document& data) const
{
	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		Send(mPlayers[i].GetClient(), data);
	}
}

void SnakeCyclesService::SetPlayerName(Player& player, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "snakecycles")
	{
		assert(data["name"].IsString());
		player.SetName(data["name"].GetString());
	}
}

// Wait
void SnakeCyclesService::OnEnterWait(int nPrevState)
{
	LOG("SnakeCyclesService::OnEnterWait()");
	mWinner = kPlayerMax;
}

void SnakeCyclesService::OnUpdateWait(double elapsed)
{
	if (mPlayers.size() >= kMinPlayers)
	{
		mFSM.SetState(kStateCountdown);
	}
}

void SnakeCyclesService::OnRecvWait(Player& player, rapidjson::Document& data)
{
}

void SnakeCyclesService::OnLeaveWait(int nNextState)
{
	LOG("SnakeCyclesService::OnLeaveWait()");
}

// Countdown
void SnakeCyclesService::SendCountdown() const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "countdown", data.GetAllocator());
	data.AddMember("number", mCountdownSent, data.GetAllocator());
	Broadcast(data);
}

void SnakeCyclesService::OnEnterCountdown(int nPrevState)
{
	LOG("SnakeCyclesService::OnEnterCountdown()");
	mCoutdownRemaing = 1;
	mCountdownSent = kInitCountdown;
	SendCountdown();
}

void SnakeCyclesService::OnUpdateCountdown(double elapsed)
{
	mCoutdownRemaing -= elapsed;

	if (mCoutdownRemaing <= 0)
	{
		assert(mCountdownSent > 0);

		mCoutdownRemaing = 1;
		--mCountdownSent;

		if (mCountdownSent == 0) 
		{
			mFSM.SetState(kStatePlay);
		}
		else
		{
			SendCountdown();
		}
	}
}

void SnakeCyclesService::OnRecvCountdown(Player& player, rapidjson::Document& data)
{
}

void SnakeCyclesService::OnLeaveCountdown(int nNextState)
{
	LOG("SnakeCyclesService::OnLeaveCountdown()");
}


// Play
void SnakeCyclesService::SendPlay() const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "play", data.GetAllocator());
	// add game data..
	Broadcast(data);
}

void SnakeCyclesService::OnEnterPlay(int nPrevState)
{
	LOG("SnakeCyclesService::OnEnterPlay()");

	assert(mWinner == kPlayerMax);

	for(size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		mPlayers[i].SetIndex(static_cast<PlayerIndex>(i));
	}


	// Init Player pos, dir, velocity and broadcast!

	SendPlay();
}

void SnakeCyclesService::OnUpdatePlay(double elapsed)
{
}

void SnakeCyclesService::OnRecvPlay(Player& player, rapidjson::Document& data)
{
}

void SnakeCyclesService::OnLeavePlay(int nNextState)
{
	LOG("SnakeCyclesService::OnLeavePlay()");
}


// End
SnakeCyclesService::PlayerIndex SnakeCyclesService::FindWinner() const
{
	auto itor = std::find_if(mPlayers.begin(), mPlayers.end(), [](const Player& player){ return player.GetState() == Player::kStateAlive; } );
	if (itor != mPlayers.end())
	{
		return itor->GetIndex();
	}

	return kPlayerMax;
}

void SnakeCyclesService::SendWinner(PlayerIndex winner) const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "winner", data.GetAllocator());
	data.AddMember("winner", static_cast<int>(winner), data.GetAllocator());
	Broadcast(data);
}

void SnakeCyclesService::SendWait() const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "wait", data.GetAllocator());
	Broadcast(data);
}

void SnakeCyclesService::OnEnterEnd(int nPrevState)
{
	LOG("SnakeCyclesService::OnEnterEnd()");

	mWinner = FindWinner();
	SendWinner(mWinner);
}

void SnakeCyclesService::OnUpdateEnd(double elapsed)
{
	// winner is gone.
	if (mWinner == kPlayerMax)
	{
		mFSM.SetState(kStateWait);
	}
}

void SnakeCyclesService::OnRecvEnd(Player& player, rapidjson::Document& data)
{
	if (player.GetIndex() == mWinner)
	{
		assert(data["type"].IsString());
		std::string type(data["type"].GetString());
		if (type == "snakecycles")
		{
			assert(data["cmd"].IsString());
			std::string cmd(data["cmd"].GetString());
			if (cmd == "restart")
			{
				mFSM.SetState(kStateWait);
			}
		}
	}
}

void SnakeCyclesService::OnLeaveEnd(int nNextState)
{
	LOG("SnakeCyclesService::OnLeaveEnd()");
	assert(nNextState == kStateWait);
	SendWait();
}
