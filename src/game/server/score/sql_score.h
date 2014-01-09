/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
/* CSqlScore Class by Sushi Tee*/
#ifndef GAME_SERVER_SQLSCORE_H
#define GAME_SERVER_SQLSCORE_H

#include <mysql_connection.h>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include "../score.h"

struct CPointsInfo
{
	char m_aMapName[128];
	unsigned int m_Points;
};

class CSqlScore: public IScore
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;

	sql::Driver *m_pDriver;
	sql::Connection *m_pConnection;
	sql::Statement *m_pStatement;
	sql::ResultSet *m_pResults;

	// copy of config vars
	const char* m_pDatabase;
	const char* m_pPrefix;
	const char* m_pUser;
	const char* m_pPass;
	const char* m_pIp;
	char m_aMap[64];
	int m_Port;

	CGameContext *GameServer()
	{
		return m_pGameServer;
	}
	IServer *Server()
	{
		return m_pServer;
	}

	static void LoadPointMapListThread(void *pUser);
	static void LoadScoreThread(void *pUser);
	static void SaveScoreThread(void *pUser);
	static void SaveTeamScoreThread(void *pUser);
	static void ShowRankThread(void *pUser);
	static void ShowTop5Thread(void *pUser);
	static void ShowTeamRankThread(void *pUser);
	static void ShowTeamTop5Thread(void *pUser);
	static void ShowTimesThread(void *pUser);
	static void ShowPointsThread(void *pUser);
	static void ShowTopPointsThread(void *pUser);

	void Init();

	bool Connect();
	void Disconnect();

	// anti SQL injection
	void ClearString(char *pString);

	void NormalizeMapname(char *pString);

public:

	CSqlScore(CGameContext *pGameServer);
	~CSqlScore();

	CPointsInfo* m_PointsInfos;
	unsigned int m_PointsSize;

	virtual void LoadPointMapList();

	virtual void LoadScore(int ClientID);
	virtual void MapPoints(int ClientID, const char* MapName);
	virtual void SaveScore(int ClientID, float Time,
			float CpTime[NUM_CHECKPOINTS]);
	virtual void SaveTeamScore(int* aClientIDs, unsigned int Size, float Time);
	virtual void ShowRank(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTeamRank(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTimes(int ClientID, const char* pName, int Debut = 1);
	virtual void ShowTimes(int ClientID, int Debut = 1);
	virtual void ShowTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowTeamTop5(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	virtual void ShowPoints(int ClientID, const char* pName, bool Search = false);
	virtual void ShowTopPoints(IConsole::IResult *pResult, int ClientID,
			void *pUserData, int Debut = 1);
	static void agoTimeToString(int agoTime, char agoString[]);
};

struct CSqlScoreData
{
	CSqlScore *m_pSqlData;
	int m_ClientID;
#if defined(CONF_FAMILY_WINDOWS)
	char m_aName[16]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aName[MAX_NAME_LENGTH * 2 - 1];
#endif

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

struct CSqlTeamScoreData
{
	CSqlScore *m_pSqlData;
	unsigned int m_Size;
	int m_aClientIDs[MAX_CLIENTS];
#if defined(CONF_FAMILY_WINDOWS)
	char m_aNames[16][MAX_CLIENTS]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aNames[MAX_NAME_LENGTH * 2 - 1][MAX_CLIENTS];
#endif

	float m_Time;
	float m_aCpCurrent[NUM_CHECKPOINTS];
	int m_Num;
	bool m_Search;
	char m_aRequestingPlayer[MAX_NAME_LENGTH];
};

#endif
