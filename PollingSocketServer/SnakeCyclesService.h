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

		kPlayerMax,
	};

	enum Color
	{	
		kRed = 0,	// kPlayer1
		kBlue,		// kPlayer2
		kGreen,		// kPlayer3
		kWhite,		// kPlayer4

		kNone,
	};

	enum
	{
		kMinPlayers = 2,
		kMaxPlayers = 4,
	};

	enum CellCount
	{
		kCellRows = 50,
		kCellColumns = 50,
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
		Player(PollingSocket* client) : mClient(client), mName(), mState(kStateAlive) {}

	public:
		PollingSocket* GetClient() const { return mClient; }

		void SetName(const char* name) { mName = name; }
		const char* GetName() const { return mName.c_str(); }

		void SetState(State state) { mState = state; }
		State GetState() const { return mState; }

		void SetIndex(PlayerIndex index) { mIndex = index; }
		PlayerIndex GetIndex() const { return mIndex; }

	private:
		PollingSocket* mClient;
		std::string mName;
		State mState;
		PlayerIndex mIndex;
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
	void SendPlay() const;
	PlayerIndex FindWinner() const;
	void SendWinner(PlayerIndex winner) const;
	void SendWait() const;

private:
	typedef std::vector<Player> PlayerList;
	PlayerList mPlayers;

	Color mBoard[kCellRows][kCellColumns];

	FSM mFSM;

	double mCoutdownRemaing;
	int mCountdownSent;

	PlayerIndex mWinner;
};
