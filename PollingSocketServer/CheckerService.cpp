#include "CheckerService.h"

#include <algorithm>

#include "Server.h"
#include "Log.h"

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <boost/bind.hpp>

#include <algorithm>
#include <functional>

namespace Checker
{
	enum Vert
	{
		UP = 0,
		DOWN,
	};

	enum Horz
	{
		LEFT = 0,
		RIGHT,
	};

	enum
	{
		MAX_ROW = 8,
		MAX_COL = 8,
	};

	int GeTargetIndex(Vert vert, Horz horz, Block::Color color, int row, int col, const std::vector<Block>& blocks)
	{
		assert(row >= 0 && row < MAX_ROW);
		assert(col >= 0 && col < MAX_COL);
		assert(blocks.size() == MAX_ROW*MAX_COL);

		int targetRow = (vert == UP) ? row - 1 : row + 1;
		int targetCol = (horz == LEFT) ? col - 1 :  col + 1;

		if( targetRow >= 0 && targetCol >= 0 &&	targetRow < MAX_ROW && targetCol < MAX_COL )
		{
			int targetIndex = targetRow*MAX_COL + targetCol;
			const Block& block = blocks[targetIndex];

			if( block.GetColor() == color )
			{
				return targetIndex;
			}
		}

		return -1;
	}

	class IsJump : public std::unary_function<Move, bool>
	{
	public:
		IsJump(int from = -1 /* check all */) : m_From(from) {}

		bool operator()(const Move& move) const
		{
			if( m_From != -1 && move.GetFrom() != m_From )
			{
				return false;
			}
			return move.GetType() == Move::JUMP;
		}

	private:
		int m_From;
	};


	Player::Player(void)
	: m_Client(NULL), m_Color(Block::EMPTY)
	{
	}

	void Player::Init(Block::Color color, std::vector<Block>& blocks)
	{
		assert(color != Block::EMPTY);
		assert(blocks.size() == Checker::MAX_ROW*Checker::MAX_COL);

		m_Client = NULL;
		m_Name.clear();

		m_Color = color;

		int row = (m_Color == Block::WHITE) ? 0 : Checker::MAX_ROW-3;
		int maxRow = (m_Color == Block::WHITE) ? 3 : Checker::MAX_ROW;
		int col = (m_Color == Block::WHITE) ? 1 : 0;
	
		for( ; row < maxRow ; ++row)
		{
			for( ; col < Checker::MAX_COL ; col += 2)
			{
				blocks[row*Checker::MAX_COL + col].SetColor(m_Color);
			}

			col = (col%2 == 0) ? 1 : 0;
		}
	}

	void Player::UpdatePossibleMoves(const std::vector<Block>& blocks)
	{
		m_PossibleMoves.clear();

		Block::Color opponetColor = (m_Color == Block::WHITE) ? Block::RED : Block::WHITE;
		Vert vert = (m_Color == Block::WHITE) ? DOWN : UP;

		for(int row = 0 ; row < Checker::MAX_ROW ; ++row)
		{
			for(int col = 0 ; col < Checker::MAX_COL ; ++col)
			{
				int curIndex = row*Checker::MAX_COL + col;
				const Block& block = blocks[curIndex];

				if( block.GetColor() == m_Color )
				{
					int target = -1;
					int victim = -1;

					target = GeTargetIndex(vert, LEFT, Block::EMPTY, row, col, blocks);
					if( target >= 0 )
					{
						m_PossibleMoves.push_back(Move(Move::NORMAL, curIndex, target));
					}

					target = GeTargetIndex(vert, RIGHT, Block::EMPTY, row, col, blocks);
					if( target >= 0 )
					{
						m_PossibleMoves.push_back(Move(Move::NORMAL, curIndex, target));
					}

					victim = GeTargetIndex(vert, LEFT, opponetColor, row, col, blocks);
					target = victim >= 0 ? GeTargetIndex(vert, LEFT, Block::EMPTY, vert == UP ? row-1 : row+1, col-1, blocks) : -1;
					if( target >= 0 )
					{
						m_PossibleMoves.push_back(Move(Move::JUMP, curIndex, target, victim));
					}

					victim = GeTargetIndex(vert, RIGHT, opponetColor, row, col, blocks);
					target = victim >= 0 ? GeTargetIndex(vert, RIGHT, Block::EMPTY, vert == UP ? row-1 : row+1, col+1, blocks) : -1;
					if( target >= 0 )
					{
						m_PossibleMoves.push_back(Move(Move::JUMP, curIndex, target, victim));
					}
				}
			}
		}

		if( m_PossibleMoves.end() != find_if(m_PossibleMoves.begin(), m_PossibleMoves.end(), IsJump()) )
		{
			m_PossibleMoves.erase( remove_if(m_PossibleMoves.begin(), m_PossibleMoves.end(), not1(IsJump())), m_PossibleMoves.end() );
		}
	}

	const Move& Player::DoMove(size_t moveIndex, std::vector<Block>& blocks) const
	{
		assert(moveIndex < m_PossibleMoves.size());

		const Move& move = m_PossibleMoves[moveIndex];

		blocks[ move.GetFrom() ].SetColor(Block::EMPTY);
		blocks[ move.GetTo() ].SetColor(m_Color);

		if( move.GetType() == Move::JUMP )
		{
			blocks[ move.GetVictim() ].SetColor(Block::EMPTY); 
		}

		return move;
	}

	bool Player::IsEliminated(const std::vector<Block>& blocks) const 
	{
		for(size_t i = 0 ; i < blocks.size() ; ++i)
		{
			if( blocks[i].GetColor() == m_Color )
			{
				return false;
			}
		}
		return true;
	}

	bool Player::IsStaleMate() const
	{
		return m_PossibleMoves.empty(); 
	}

	bool Player::CanJump(int from) const
	{
		assert(!m_PossibleMoves.empty());

		return m_PossibleMoves.end() != find_if(m_PossibleMoves.begin(), m_PossibleMoves.end(), IsJump(from));
	}

	void Player::LimitPossibleJumps(int from)
	{
		assert(!m_PossibleMoves.empty());

		m_PossibleMoves.erase( remove_if(m_PossibleMoves.begin(), m_PossibleMoves.end(), not1(IsJump(from))), m_PossibleMoves.end() );
	}

	int Player::GetPossibleMove(int from, int to) const
	{
		for( size_t i = 0; i < m_PossibleMoves.size() ; ++i)
		{
			const Move& move = m_PossibleMoves[i];

			if( move.GetFrom() == from && move.GetTo() == to )
			{
				return i;
			}
		}

		return -1;
	}
} // Checker

using namespace Checker;

/*static*/ CheckerService::ServiceList CheckerService::sServices;

/*static*/ void CheckerService::Init()
{
	LOG("CheckerService::Init()");
}

/*static*/ void CheckerService::Shutdown()
{
	LOG("CheckerService::Shutdown()");

	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		delete sServices[i];
	}
	sServices.clear();
}


/*static*/ void CheckerService::Update()
{
	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		sServices[i]->UpdateInternal();
	}

	Flush();
}

/*static*/ void CheckerService::OnRecv(PollingSocket* client, rapidjson::Document& data)
{
	if (CreateOrEnter(client, data))
	{
		return;
	}

	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		sServices[i]->OnRecvInternal(client, data);
	}
}

/*static*/ void CheckerService::RemoveClient(PollingSocket* client)
{
	for (size_t i = 0 ; i < sServices.size() ; ++i)
	{
		if (!sServices[i]->RemoveClientInternal(client))
		{
			return;
		}
	}
}

/*static*/ bool CheckerService::CreateOrEnter(PollingSocket* client, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "service_create")
	{
		assert(data["name"].IsString());
		std::string name(data["name"].GetString());

		if (name == "checker")
		{
			for (size_t i = 0 ; i < sServices.size() ; ++i)
			{
				CheckerService* service = sServices[i];

				if (service->mFSM.GetState() == kStateWait && service->GetNumberOfPlayers() < 2)
				{
					service->AddClient(client);
					return true;
				}
			}

			CheckerService* newService = new CheckerService;
			newService->AddClient(client);
			sServices.push_back(newService);
			return true;
		}
	}
	return false;
}

/*static*/ void CheckerService::Flush()
{
	for (auto itor = sServices.begin() ; itor != sServices.end() ; )
	{
		CheckerService* service = *itor;

		if (service->mFSM.GetState() == kStateWait && service->GetNumberOfPlayers() == 0)
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

CheckerService::CheckerService(void)
	: m_Blocks(MAX_ROW*MAX_COL)
{
	InitFSM();
}


CheckerService::~CheckerService(void)
{
	ShutdownFSM();
}

void CheckerService::InitFSM()
{
#define BIND_CALLBACKS(State) boost::bind(&CheckerService::OnEnter##State, this, _1), \
							  boost::bind(&CheckerService::DummyUpdate, this), \
							  boost::bind(&CheckerService::OnLeave##State, this, _1)

	mFSM.RegisterState(kStateWait, BIND_CALLBACKS(Wait));
	mFSM.RegisterState(kStatePlayer1Turn, BIND_CALLBACKS(Player1Turn));
	mFSM.RegisterState(kStatePlayer2Turn, BIND_CALLBACKS(Player2Turn));
	mFSM.RegisterState(kStateCheckResult, BIND_CALLBACKS(CheckResult));
	mFSM.RegisterState(kStateGameCanceled, BIND_CALLBACKS(GameCanceled));

#undef BIND_CALLBACKS

	mFSM.SetState(kStateWait);
}

void CheckerService::ShutdownFSM()
{
	mFSM.Reset(false);
}


void CheckerService::UpdateInternal()
{
	CheckPlayerConnection();

	mFSM.Update();
}


void CheckerService::OnRecvInternal(PollingSocket* client, rapidjson::Document& data)
{
	switch(mFSM.GetState())
	{
	case kStateWait:			OnUpdateWait(client, data);			break;
	case kStatePlayer1Turn:		OnUpdatePlayer1Turn(client, data);	break;
	case kStatePlayer2Turn:		OnUpdatePlayer2Turn(client, data);	break;
	case kStateCheckResult:		OnUpdateCheckResult(client, data);	break;
	case kStateGameCanceled:	OnUpdateGameCanceled(client, data);	break;

	default:
		assert(0);
		return;
	}
}


void CheckerService::AddClient(PollingSocket* client)
{
	assert(mFSM.GetState() == kStateWait);

	if (mPlayer1.GetClient() == NULL)
	{
		mPlayer1.SetClient(client);
	}
	else if (mPlayer2.GetClient() == NULL)
	{
		mPlayer2.SetClient(client);
	}
}


bool CheckerService::RemoveClientInternal(PollingSocket* client)
{
	if (mPlayer1.GetClient() == client)
	{
		mPlayer1.SetClient(NULL);
		return true;
	}
	else if (mPlayer2.GetClient() == client)
	{
		mPlayer2.SetClient(NULL);
		return true;
	}

	return false;
}


void CheckerService::CheckPlayerConnection()
{
	if (mFSM.GetState() == kStateWait)
	{
		if (mPlayer1.GetClient() == NULL && mPlayer2.GetClient() == NULL)
		{
			mFSM.SetState(kStateGameCanceled);
		}
	}
	else
	{
		if (mPlayer1.GetClient() == NULL || mPlayer2.GetClient() == NULL)
		{
			mFSM.SetState(kStateGameCanceled);
		}
	}
}


void CheckerService::Send(PollingSocket* client, rapidjson::Document& data)
{
	if (client)
	{
		client->AsyncSend(data);
	}
}


void CheckerService::Broadcast(rapidjson::Document& data)
{
	Send(mPlayer1.GetClient(), data);
	Send(mPlayer2.GetClient(), data);
}

void CheckerService::SetPlayerName(Player& player, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "checker")
	{
		assert(data["name"].IsString());
		player.SetName(data["name"].GetString());
	}
}

// Wait
void CheckerService::OnEnterWait(int nPrevState)
{
	LOG("CheckerService::OnEnterWait()");

	mPlayer1.Init(Block::WHITE, m_Blocks);
	mPlayer2.Init(Block::RED, m_Blocks);
	
	for (size_t i = 0 ; i < m_Blocks.size() ; ++i)
	{
		m_Blocks[i].SetColor(Block::EMPTY);
	}
}

void CheckerService::OnUpdateWait(PollingSocket* client, rapidjson::Document& data)
{
	if (mPlayer1.GetClient() == client)
	{
		SetPlayerName(mPlayer1, data);
	}
	else if (mPlayer2.GetClient() == client)
	{
		SetPlayerName(mPlayer2, data);
	}
	else
	{
		return;
	}

	if (!mPlayer1.GetName().empty() && !mPlayer2.GetName().empty())
	{
		rapidjson::Document playerData;
		playerData.SetObject();
		playerData.AddMember("type", "checker", playerData.GetAllocator());
		playerData.AddMember("subtype", "setplayers", playerData.GetAllocator());
		playerData.AddMember("player1_name", mPlayer1.GetName().c_str(), playerData.GetAllocator());
		playerData.AddMember("player2_name", mPlayer2.GetName().c_str(), playerData.GetAllocator());

		playerData.AddMember("assigned_to", 1, playerData.GetAllocator());
		Send(mPlayer1.GetClient(), playerData);

		playerData["assigned_to"] = 2;
		Send(mPlayer2.GetClient(), playerData);

		mFSM.SetState(kStatePlayer1Turn);
	}
}

void CheckerService::OnLeaveWait(int nNextState)
{
	LOG("CheckerService::OnLeaveWait()");
}


void CheckerService::SetPlayerTurn(int playerTurn)
{
	m_CurrentTurn = playerTurn;

	Player& player = (m_CurrentTurn == PLAYER_1 ? mPlayer1 : mPlayer2);

	player.UpdatePossibleMoves(m_Blocks);

	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "checker", data.GetAllocator());
	data.AddMember("subtype", "setturn", data.GetAllocator());
	data.AddMember("player", playerTurn, data.GetAllocator());
	Broadcast(data);
}

void CheckerService::CheckPlayerMove(Player& player, rapidjson::Document& data)
{
	assert(data["type"].IsString());
	std::string type(data["type"].GetString());
	if (type == "checker")
	{
		assert(data["from"].IsInt());
		int from = data["from"].GetInt();

		assert(data["to"].IsInt());
		int to = data["to"].GetInt();

		int moveIndex = player.GetPossibleMove(from, to);
		if (moveIndex >= 0)
		{
			m_LastMove = player.DoMove(moveIndex, m_Blocks);

			rapidjson::Document data;
			data.SetObject();
			data.AddMember("type", "checker", data.GetAllocator());
			data.AddMember("subtype", "move", data.GetAllocator());
			data.AddMember("player", m_CurrentTurn == PLAYER_1 ? 1 : 2, data.GetAllocator());
			data.AddMember("from", m_LastMove.GetFrom(), data.GetAllocator());
			data.AddMember("to", m_LastMove.GetTo(), data.GetAllocator());
			data.AddMember("victim", m_LastMove.GetVictim(), data.GetAllocator());
			Broadcast(data);

			mFSM.SetState(kStateCheckResult);
		}
		else
		{
			LOG("CheckerService::CheckPlayerMove() - from[%d] / to[%d] is invalid. ignored.", from, to);
		}
	}
}


void CheckerService::OnEnterPlayer1Turn(int nPrevState)
{
	LOG("CheckerService::OnEnterPlayer1Turn()");
	SetPlayerTurn(1);
}

void CheckerService::OnUpdatePlayer1Turn(PollingSocket* client, rapidjson::Document& data)
{
	if (mPlayer1.GetClient() == client)
	{
		CheckPlayerMove(mPlayer1, data);
	}
	else
	{
		return;
	}
}

void CheckerService::OnLeavePlayer1Turn(int nNextState)
{
	LOG("CheckerService::OnLeavePlayer1Turn()");
}


void CheckerService::OnEnterPlayer2Turn(int nPrevState)
{
	LOG("CheckerService::OnEnterPlayer2Turn()");
	SetPlayerTurn(2);
}

void CheckerService::OnUpdatePlayer2Turn(PollingSocket* client, rapidjson::Document& data)
{
	if (mPlayer2.GetClient() == client)
	{
		CheckPlayerMove(mPlayer2, data);
	}
	else
	{
		return;
	}
}

void CheckerService::OnLeavePlayer2Turn(int nNextState)
{
	LOG("CheckerService::OnLeavePlayer2Turn()");
}

void CheckerService::SetGameEnd(int winner)
{
	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "checker", data.GetAllocator());
	data.AddMember("subtype", "result", data.GetAllocator());

	switch(winner)
	{
	case PLAYER_1:		data.AddMember("winner", 1, data.GetAllocator());	break;
	case PLAYER_2:		data.AddMember("winner", 2, data.GetAllocator());	break;
	case MAX_PLAYER:	data.AddMember("winner", -1, data.GetAllocator());	break;

	default:
		assert(0);
		return;
	}

	Broadcast(data);

	mFSM.SetState(kStateWait);
}


int CheckerService::FindWinner() const
{
	if (m_CurrentTurn == PLAYER_1)
	{
		if (mPlayer2.IsEliminated(m_Blocks))
		{
			return PLAYER_1;
		}
	}
	else
	{
		assert(m_CurrentTurn == PLAYER_2);
		if (mPlayer1.IsEliminated(m_Blocks))
		{
			return PLAYER_2;
		}
	}

	if (mPlayer1.IsStaleMate())
	{
		if (mPlayer2.IsStaleMate())
		{
			return MAX_PLAYER;
		}
		else
		{
			return PLAYER_2;
		}
	}
	else if (mPlayer2.IsStaleMate())
	{
		return PLAYER_1;
	}

	return INVALID_PLAYER;
}


int CheckerService::FindNextTurn()
{
	Player& curPlayer = (m_CurrentTurn == PLAYER_1 ? mPlayer1 : mPlayer2);

	if( m_LastMove.GetType() == Move::JUMP && curPlayer.CanJump(m_LastMove.GetTo()) )
	{
		// The current player should keep jumping with the same piece.
		curPlayer.LimitPossibleJumps(m_LastMove.GetTo());
		return m_CurrentTurn;
	}

	return (m_CurrentTurn == PLAYER_1) ? PLAYER_2 : PLAYER_1;
}


void CheckerService::OnEnterCheckResult(int nPrevState)
{
	LOG("CheckerService::OnEnterCheckResult()");

	int winner = FindWinner();

	switch(winner)
	{
	case INVALID_PLAYER: 
		{
			int nextPlayer = FindNextTurn();
			mFSM.SetState(nextPlayer == PLAYER_1 ? kStatePlayer1Turn : kStatePlayer2Turn);
		}
		break;

	case PLAYER_1:
	case PLAYER_2:
	case MAX_PLAYER:
		SetGameEnd(winner);	
		break;

	default:
		assert(0);
		return;
	}
}

void CheckerService::OnUpdateCheckResult(PollingSocket* client, rapidjson::Document& data) 
{
}

void CheckerService::OnLeaveCheckResult(int nNextState)
{
	LOG("CheckerService::OnLeaveCheckResult()");
}


void CheckerService::OnEnterGameCanceled(int nPrevState)
{
	LOG("CheckerService::OnEnterGameCanceled()");

	rapidjson::Document data;
	data.SetObject();
	data.AddMember("type", "checker", data.GetAllocator());
	data.AddMember("subtype", "canceled", data.GetAllocator());
	Broadcast(data);

	mFSM.SetState(kStateWait);
}


void CheckerService::OnUpdateGameCanceled(PollingSocket* client, rapidjson::Document& data)
{
}

void CheckerService::OnLeaveGameCanceled(int nNextState)
{
	LOG("CheckerService::OnLeaveGameCanceled()");
}

int CheckerService::GetNumberOfPlayers() const
{
	int count = 0;
	count += (mPlayer1.GetClient() == NULL ? 0 : 1);
	count += (mPlayer2.GetClient() == NULL ? 0 : 1);
	return count;
}