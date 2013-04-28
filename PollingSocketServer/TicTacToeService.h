#pragma once

#include <vector>
#include <rapidjson/document.h>

#include "FSM.h"


class PollingSocket;

class TicTacToeService
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
	typedef std::vector<TicTacToeService*> ServiceList;
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

	struct Player
	{
		Player() : client(NULL) {}

		PollingSocket* client;
		std::string name;
	};

	enum Symbol
	{
		kSymbolNone = 0, 
		kSymbolOOO,
		kSymbolXXX, 
	};

	enum CellCount
	{
		kCellRows = 3,
		kCellColumns = 3,
	};

private:
	TicTacToeService(void);
	~TicTacToeService(void);

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

	void DummyUpdate(double) {}

	void CheckPlayerConnection();

	void SetPlayerName(Player& player, rapidjson::Document& data);
	void SetPlayerTurn(int playerTurn);
	void CheckPlayerMove(Player& player, Symbol symbol, rapidjson::Document& data);
	void SetGameEnd(Symbol winning);

	bool CheckRowStraight(int col, Symbol symbol);
	bool CheckColStraight(int row, Symbol symbol);
	bool CheckSlashStraight(Symbol symbol);
	bool CheckBackSlashStraight(Symbol symbol);
	bool CheckBoardIsFull();

	void Send(PollingSocket* client, rapidjson::Document& data);
	void Broadcast(rapidjson::Document& data);

private:
	typedef std::vector<PollingSocket*> ClientList;
	ClientList m_Clients;

	FSM mFSM;
	Player mPlayer1;
	Player mPlayer2;
	
	Symbol mBoard[kCellRows][kCellColumns];

	int mLastMoveRow;
	int mLastMoveCol;
};
