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
	const double kTimePerBlock = 1;
}


SnakeCyclesService::Player::Player(PollingSocket* client) 
	: mClient(client)
	, mName()
	, mState(kStateDead) 
	, mIndex(kPlayerNone)
	, mTimeRemaing(kTimePerBlock)
	, mDirection(kDOWN)
	, mPos()
{
}

void SnakeCyclesService::Player::Init(PlayerIndex index, PlayerIndex* board, int numRows, int numCols)
{
	assert(board);

	mState = kStateAlive;
	mIndex = index;	
	mTimeRemaing = kTimePerBlock;

	switch(mIndex)
	{
	case kPlayer1:
		// top
		mPos.x = numCols/2;
		mPos.y = numRows-1;
		mDirection = kDOWN;
		break;

	case kPlayer2:
		// right
		mPos.x = numCols-1;
		mPos.y = numRows/2;
		mDirection = kLEFT;
		break;

	case kPlayer3:
		// bottom
		mPos.x = numCols/2;
		mPos.y = 0;
		mDirection = kUP;
		break;

	case kPlayer4:
		// left
		mPos.x = 0;
		mPos.y = numRows/2;
		mDirection = kRIGHT;
		break;

	default:
		assert(0);
		return;
	}
}

bool SnakeCyclesService::Player::Move(double elapsed, PlayerIndex* board, int numRows, int numCols, Wall& wall)
{
	if (mState == kStateDead)
	{
		return false;
	}

	mTimeRemaing -= elapsed;
	if (mTimeRemaing <= 0)
	{
		mTimeRemaing = kTimePerBlock;

		wall.pos = mPos;
		wall.playerIndex = mIndex;

		switch(mDirection)
		{
		case kUP:		mPos.y += 1;	break;
		case kDOWN:		mPos.y -= 1;		break;
		case kLEFT:		mPos.x -= 1;		break;
		case kRIGHT:	mPos.x += 1;	break;

		default:
			assert(0);
			return false;
		}

		int index = wall.pos.y * numCols + wall.pos.x;
		board[index] = mIndex;
		return true;
	}
	return false;
}


void SnakeCyclesService::Player::CheckCollision(const std::vector<Player>& players, PlayerIndex* board, int numRows, int numCols)
{
	if (mPos.x < 0 || mPos.y < 0 || mPos.x >= numCols || mPos.y >= numRows)
	{
		mState = kStateDead;
	}
	else
	{
		int curIndex = mPos.y * numCols + mPos.x;
		if (board[curIndex] != kPlayerNone)
		{
			mState = kStateDead;
		}
		else
		{
			for(size_t i = 0 ; i < players.size() ; ++i)
			{
				if (mIndex == players[i].mIndex)
				{
					continue;
				}

				if (mPos.x == players[i].mPos.x && mPos.y == players[i].mPos.y)
				{
					mState = kStateDead;
					break;
				}
			}
		}
	}
}


void SnakeCyclesService::Player::GetStatus(rapidjson::Value& outData, rapidjson::Document::AllocatorType& allocator) const
{
	outData.SetObject();
	outData.AddMember("index", static_cast<int>(mIndex), allocator);
	outData.AddMember("x", mPos.x, allocator);
	outData.AddMember("y", mPos.y, allocator);
	outData.AddMember("dir", static_cast<int>(mDirection), allocator);
	outData.AddMember("state", static_cast<int>(mState), allocator);
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
	, mWinner(kPlayerNone)
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
			mWinner = kPlayerNone;
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
	mWinner = kPlayerNone;
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
void SnakeCyclesService::SendPlayerIndex(const Player& player) const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "playerindex", data.GetAllocator());
	data.AddMember("playerindex", static_cast<int>(player.GetIndex()), data.GetAllocator());

	Send(player.GetClient(), data);
}

void SnakeCyclesService::SendPlay() const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "play", data.GetAllocator());

	rapidjson::Value players(rapidjson::kArrayType);
	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		rapidjson::Value playerStatus(rapidjson::kObjectType);
		mPlayers[i].GetStatus(playerStatus, data.GetAllocator());
		players.PushBack(playerStatus, data.GetAllocator());
	}

	data.AddMember("players", players, data.GetAllocator());

	Broadcast(data);
}

void SnakeCyclesService::SendMove(const std::vector<Wall>& newWalls) const
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "snakecycles", data.GetAllocator());
	data.AddMember("subtype", "move", data.GetAllocator());

	// add new wallls
	rapidjson::Value walls(rapidjson::kArrayType);
	for (size_t i = 0 ; i < newWalls.size() ; ++i)
	{			
		rapidjson::Value wall(rapidjson::kObjectType);

		wall.AddMember("x", newWalls[i].pos.x, data.GetAllocator());
		wall.AddMember("y", newWalls[i].pos.y, data.GetAllocator());
		wall.AddMember("playerIndex", static_cast<int>(newWalls[i].playerIndex), data.GetAllocator());

		walls.PushBack(wall, data.GetAllocator());
	}
	data.AddMember("walls", walls, data.GetAllocator());

	// update player status
	rapidjson::Value players(rapidjson::kArrayType);

	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		rapidjson::Value playerStatus(rapidjson::kObjectType);
		mPlayers[i].GetStatus(playerStatus, data.GetAllocator());
		players.PushBack(playerStatus, data.GetAllocator());
	}
	data.AddMember("players", players, data.GetAllocator());

	Broadcast(data);
}

void SnakeCyclesService::OnEnterPlay(int nPrevState)
{
	LOG("SnakeCyclesService::OnEnterPlay()");

	assert(mWinner == kPlayerNone);

	for (size_t i = 0 ; i < kCellRows*kCellColumns ; ++i)
	{
		mBoard[i] = kPlayerNone;
	}

	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		mPlayers[i].Init(static_cast<PlayerIndex>(i), mBoard, kCellRows, kCellColumns);
	}

	// send player index
	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		SendPlayerIndex(mPlayers[i]);
	}

	// send play!
	SendPlay();
}

void SnakeCyclesService::OnUpdatePlay(double elapsed)
{
	std::vector<Wall> newWalls;
	for (size_t i = 0 ; i < mPlayers.size() ; ++i)
	{
		Wall wall;
		if (mPlayers[i].Move(elapsed, mBoard, kCellColumns, kCellColumns, wall))
		{
			newWalls.push_back(wall);
		}
	}

	if (!newWalls.empty())
	{
		for (size_t i = 0 ; i < mPlayers.size() ; ++i)
		{
			mPlayers[i].CheckCollision(mPlayers, mBoard, kCellColumns, kCellColumns);
		}

		SendMove(newWalls);
	}

	// Check Game End
}

void SnakeCyclesService::OnRecvPlay(Player& player, rapidjson::Document& data)
{
	// Input handling
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "snakecycles")
	{
		assert(data["subtype"].IsString());
		std::string subtype(data["subtype"].GetString());
		if (subtype == "dir")
		{
			assert(data["dir"].IsInt());
			int dir = data["dir"].GetInt();

			player.SetDir(static_cast<Direction>(dir));
		}
	}
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

	return kPlayerNone;
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
	if (mWinner == kPlayerNone)
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
			assert(data["subtype"].IsString());
			std::string subtype(data["subtype"].GetString());
			if (subtype == "restart")
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
