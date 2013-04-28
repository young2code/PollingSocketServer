#pragma once

#include <vector>
#include <rapidjson/document.h>

#include "FSM.h"


class PollingSocket;

class SnakeCyclesService
{
public:
	static void Init();
	static void Shutdown();

	static void Update();
	static void OnRecv(PollingSocket* client, rapidjson::Document& data);

	static void RemoveClient(PollingSocket* client);

private:
	static void CreateOrEnter(PollingSocket* client, rapidjson::Document& data);
	static void Flush();

private:
	typedef std::vector<SnakeCyclesService*> ServiceList;
	static ServiceList sServices;

private:
	enum State
	{
		kStateWait,
		kStateCountdown,
		kStatePlay,
		kStateEnd,
	};

	enum PlayerIndex
	{
		kPlayer1 = 0,
		kPlayer2,
		kPlayer3,
		kPlayer4,

		kPlayerNone,
	};

	enum Direction
	{
		kUP = 0,
		kDOWN,
		kLEFT,
		kRIGHT,
	};

	enum
	{
		kMinPlayers = 1, //2,
		kMaxPlayers = 4,
	};

	enum CellCount
	{
		kCellRows = 20,
		kCellColumns = 20,
	};

	struct Position
	{
		Position() : x(0), y(0) {}

		int x;
		int y;
	};

	struct Wall
	{
		Position pos;
		PlayerIndex playerIndex;
	};

	class Player
	{
	public:
		enum State
		{
			kStateAlive = 0,
			kStateDead,
		};

	public:
		Player(PollingSocket* client);

	public:
		void Init(PlayerIndex index, PlayerIndex* board, int numRows, int numCols);
		bool Move(double elapsed, PlayerIndex* board, int numRows, int numCols, Wall& wall);
		void CheckCollision(const std::vector<Player>& players, PlayerIndex* board, int numRows, int numCols);

		void GetStatus(rapidjson::Value& outData, rapidjson::Document::AllocatorType& allocator) const;

		PollingSocket* GetClient() const { return mClient; }

		void SetName(const char* name) { mName = name; }
		const char* GetName() const { return mName.c_str(); }

		void SetState(State state) { mState = state; }
		State GetState() const { return mState; }

		void SetIndex(PlayerIndex index) { mIndex = index; }
		PlayerIndex GetIndex() const { return mIndex; }

		void SetDir(Direction dir) { mDirection = dir; }

	private:
		PollingSocket* mClient;
		std::string mName;
		State mState;
		PlayerIndex mIndex;
		double mTimeRemaing;
		Direction mDirection;
		Position mPos;
	};

private:
	SnakeCyclesService(void);
	~SnakeCyclesService(void);

	void UpdateInternal();
	void OnRecvInternal(PollingSocket* client, rapidjson::Document& data);

	void OnRecvWait(Player& player, rapidjson::Document& data);
	void OnRecvCountdown(Player& player, rapidjson::Document& data);
	void OnRecvPlay(Player& player, rapidjson::Document& data);
	void OnRecvEnd(Player& player, rapidjson::Document& data);

	void AddClient(PollingSocket* client);
	bool RemoveClientInternal(PollingSocket* client);

	void InitFSM();
	void ShutdownFSM();

	void OnEnterWait(int nPrevState);
	void OnUpdateWait(double elapsed);
	void OnLeaveWait(int nNextState);

	void OnEnterCountdown(int nPrevState);
	void OnUpdateCountdown(double elapsed);
	void OnLeaveCountdown(int nNextState);

	void OnEnterPlay(int nPrevState);
	void OnUpdatePlay(double elapsed);
	void OnLeavePlay(int nNextState);

	void OnEnterEnd(int nPrevState);
	void OnUpdateEnd(double elapsed);
	void OnLeaveEnd(int nNextState);

	void CheckPlayerConnection();

	void SetPlayerName(Player& player, rapidjson::Document& data);

	void Send(PollingSocket* client, rapidjson::Document& data) const;
	void Broadcast(rapidjson::Document& data) const;

	void SendCountdown() const;
	void SendPlayerIndex(const Player& player) const;
	void SendPlay() const;
	void SendMove(const std::vector<Wall>& newWalls) const;
	PlayerIndex FindWinner() const;
	void SendWinner(PlayerIndex winner) const;
	void SendWait() const;

private:
	typedef std::vector<Player> PlayerList;
	PlayerList mPlayers;

	PlayerIndex mBoard[kCellRows*kCellColumns];

	FSM mFSM;

	double mCoutdownRemaing;
	int mCountdownSent;

	PlayerIndex mWinner;
};
