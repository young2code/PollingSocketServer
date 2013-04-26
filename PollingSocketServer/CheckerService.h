#pragma once

#include <vector>
#include <string>
#include <rapidjson/document.h>

#include "FSM.h"

class PollingSocket;

namespace Checker
{
	//--------------------------------
	class Block
	{
	public:
		enum Color
		{
			EMPTY = 0,
			WHITE,
			RED,
		};

	public:
		Block(void) : m_Color(EMPTY) {}

		void SetColor(Color color) { m_Color = color; }
		Color GetColor() const { return m_Color; }

	private:
		Color m_Color;
	};

	//--------------------------------
	class Move
	{
	public:
		enum Type
		{
			NORMAL = 0,
			JUMP,
		};

	public:
		Move() : m_Type(NORMAL), m_From(-1), m_To(-1), m_Victim(-1) {}

		Move(Type type, int from, int to, int victim = -1 /* no victim */) 
			: m_Type(type), m_From(from), m_To(to), m_Victim(victim) {}

		Type GetType() const { return m_Type; }
		int GetTo() const { return m_To; }
		int GetFrom() const { return m_From; }
		int GetVictim() const { return m_Victim; }

	private:
		Type m_Type;
		int m_From;
		int m_To;
		int m_Victim;
	};

	//--------------------------------
	class Player
	{
	public:
		Player(void);

		void Init(Block::Color color, std::vector<Block>& blocks);

		void UpdatePossibleMoves(const std::vector<Block>& blocks);

		bool IsEliminated(const std::vector<Block>& blocks) const;
		bool IsStaleMate() const;

		bool CanJump(int from) const;
		void LimitPossibleJumps(int from);

		const Move& DoMove(size_t moveIndex, std::vector<Block>& blocks) const;
		int GetPossibleMove(int from, int to) const;

		void SetClient(PollingSocket* client) { m_Client = client; }
		PollingSocket* GetClient() const { return m_Client; }

		void SetName(const char* name) { m_Name = name; }
		const std::string& GetName() const { return m_Name; }

	private:	
		PollingSocket* m_Client;
		std::string m_Name;
		Block::Color m_Color;
		std::vector<Move> m_PossibleMoves;
	};
}

class CheckerService
{

public:
	static void Init();
	static void Shutdown();

	static void Update();
	static void OnRecv(PollingSocket* client, rapidjson::Document& data);

	static void RemoveClient(PollingSocket* client);

private:
	static bool CreateOrEnter(PollingSocket* client, rapidjson::Document& data);
	static void Flush();

private:
	typedef std::vector<CheckerService*> ServiceList;
	static ServiceList sServices;

private:
	enum State
	{
		kStateWait,
		kStatePlayer1Turn,
		kStatePlayer2Turn,
		kStateCheckResult,
		kStateGameCanceled,
	};

	enum
	{
		INVALID_PLAYER = -1,

		PLAYER_1 = 0,
		PLAYER_2,

		MAX_PLAYER,
	};



private:
	CheckerService(void);
	~CheckerService(void);

	void UpdateInternal();
	void OnRecvInternal(PollingSocket* client, rapidjson::Document& data);

	void AddClient(PollingSocket* client);
	bool RemoveClientInternal(PollingSocket* client);

	void InitFSM();
	void ShutdownFSM();

	void OnEnterWait(int nPrevState);
	void OnUpdateWait(PollingSocket* client, rapidjson::Document& data);
	void OnLeaveWait(int nNextState);

	void OnEnterPlayer1Turn(int nPrevState);
	void OnUpdatePlayer1Turn(PollingSocket* client, rapidjson::Document& data);
	void OnLeavePlayer1Turn(int nNextState);

	void OnEnterPlayer2Turn(int nPrevState);
	void OnUpdatePlayer2Turn(PollingSocket* client, rapidjson::Document& data);
	void OnLeavePlayer2Turn(int nNextState);

	void OnEnterCheckResult(int nPrevState);
	void OnUpdateCheckResult(PollingSocket* client, rapidjson::Document& data);
	void OnLeaveCheckResult(int nNextState);

	void OnEnterGameCanceled(int nPrevState);
	void OnUpdateGameCanceled(PollingSocket* client, rapidjson::Document& data);
	void OnLeaveGameCanceled(int nNextState);

	void DummyUpdate() {}

	void CheckPlayerConnection();

	void SetPlayerName(Checker::Player& player, rapidjson::Document& data);
	void SetPlayerTurn(int playerTurn);
	void CheckPlayerMove(Checker::Player& player, rapidjson::Document& data);
	int FindWinner() const;
	int FindNextTurn();
	void SetGameEnd(int winner);

	int GetNumberOfPlayers() const;

	void Send(PollingSocket* client, rapidjson::Document& data);
	void Broadcast(rapidjson::Document& data);

private:
	FSM mFSM;
	Checker::Player mPlayer1;
	Checker::Player mPlayer2;
	
	std::vector<Checker::Block> m_Blocks;
	int m_CurrentTurn;
	Checker::Move m_LastMove;
};
