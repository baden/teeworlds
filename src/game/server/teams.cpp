/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include <engine/shared/config.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) :
		m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_TeamState[i] = TEAMSTATE_EMPTY;
		m_TeeFinished[i] = false;
		m_MembersCount[i] = 0;
		m_LastChat[i] = 0;
		m_TeamLocked[i] = false;
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	int Tick = Server()->Tick();
	CCharacter* pStartingChar = Character(ClientID);
	if (!pStartingChar)
		return;
	if (m_Core.Team(ClientID) != TEAM_FLOCK && pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		return;
	if (m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
	}
	else
	{
		bool Waiting = false;
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (m_Core.Team(ClientID) == m_Core.Team(i))
			{
				CPlayer* pPlayer = GetPlayer(i);
				if (pPlayer && pPlayer->IsPlaying()
						&& GetDDRaceState(pPlayer) == DDRACE_FINISHED)
				{
					Waiting = true;
					pStartingChar->m_DDRaceState = DDRACE_NONE;

					if (m_LastChat[ClientID] + Server()->TickSpeed()
							+ g_Config.m_SvChatDelay < Tick)
					{
						char aBuf[128];
						str_format(
								aBuf,
								sizeof(aBuf),
								"%s has finished and didn't go through start yet, wait for him or join another team.",
								Server()->ClientName(i));
						GameServer()->SendChatTarget(ClientID, aBuf);
						m_LastChat[ClientID] = Tick;
					}
					if (m_LastChat[i] + Server()->TickSpeed()
							+ g_Config.m_SvChatDelay < Tick)
					{
						char aBuf[128];
						str_format(
								aBuf,
								sizeof(aBuf),
								"%s wants to start a new round, kill or walk to start.",
								Server()->ClientName(ClientID));
						GameServer()->SendChatTarget(i, aBuf);
						m_LastChat[i] = Tick;
					}
				}
			}
		}

		if (m_TeamState[m_Core.Team(ClientID)] < TEAMSTATE_STARTED && !Waiting)
		{
			ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_STARTED);
			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (m_Core.Team(ClientID) == m_Core.Team(i))
				{
					CPlayer* pPlayer = GetPlayer(i);
					// TODO: THE PROBLEM IS THAT THERE IS NO CHARACTER SO START TIME CAN'T BE SET!
					if (pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
					{
						SetDDRaceState(pPlayer, DDRACE_STARTED);
						SetStartTime(pPlayer, Tick);
					}
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	if (m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		CPlayer* pPlayer = GetPlayer(ClientID);
		if (pPlayer && pPlayer->IsPlaying())
			OnFinish(pPlayer);
	}
	else
	{
		m_TeeFinished[ClientID] = true;
		if (TeamFinished(m_Core.Team(ClientID)))
		{
			ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_FINISHED); //TODO: Make it better
			//ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_OPEN);

			CPlayer *TeamPlayers[MAX_CLIENTS];
			unsigned int PlayersCount = 0;

			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (m_Core.Team(ClientID) == m_Core.Team(i))
				{
					CPlayer* pPlayer = GetPlayer(i);
					if (pPlayer && pPlayer->IsPlaying())
					{
						OnFinish(pPlayer);
						m_TeeFinished[i] = false;

						TeamPlayers[PlayersCount++] = pPlayer;
					}
				}
			}

			OnTeamFinish(TeamPlayers, PlayersCount);

		}
	}
}

bool CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	//Check on wrong parameters. +1 for TEAM_SUPER
	if (ClientID < 0 || ClientID >= MAX_CLIENTS || Team < 0
			|| Team >= MAX_CLIENTS + 1)
		return false;
	//You can join to TEAM_SUPER at any time, but any other group you cannot if it started
	if (Team != TEAM_SUPER && m_TeamState[Team] > TEAMSTATE_OPEN)
		return false;
	//No need to switch team if you there
	if (m_Core.Team(ClientID) == Team)
		return false;
	if (!Character(ClientID))
		return false;
	//You cannot be in TEAM_SUPER if you not super
	if (Team == TEAM_SUPER && !Character(ClientID)->m_Super)
		return false;
	//if you begin race
	if (Character(ClientID)->m_DDRaceState != DDRACE_NONE && Team != TEAM_SUPER)
		return false;

	SetForceCharacterTeam(ClientID, Team);

	//GameServer()->CreatePlayerSpawn(Character(id)->m_Core.m_Pos, TeamMask());
	return true;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	ForceLeaveTeam(ClientID);

	m_Core.Team(ClientID, Team);

	if (m_Core.Team(ClientID) != TEAM_SUPER)
		m_MembersCount[m_Core.Team(ClientID)]++;
	if (Team != TEAM_SUPER && m_TeamState[Team] == TEAMSTATE_EMPTY)
	{
		ChangeTeamState(Team, TEAMSTATE_OPEN);

		if (GameServer()->Collision()->m_NumSwitchers > 0) {
			for (int i = 0; i < GameServer()->Collision()->m_NumSwitchers+1; ++i)
			{
				GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = true;
				GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = 0;
				GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = TILE_SWITCHOPEN;
			}
		}
	}

	for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
	{
		if (GetPlayer(LoopClientID)
				&& GetPlayer(LoopClientID)->m_IsUsingDDRaceClient)
			SendTeamsState(LoopClientID);
	}
}

void CGameTeams::ForceLeaveTeam(int ClientID)
{
	m_TeeFinished[ClientID] = false;

	if (m_Core.Team(ClientID) != TEAM_FLOCK
			&& m_Core.Team(ClientID) != TEAM_SUPER
			&& m_TeamState[m_Core.Team(ClientID)] != TEAMSTATE_EMPTY)
	{
		bool NoOneInOldTeam = true;
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (i != ClientID && m_Core.Team(ClientID) == m_Core.Team(i))
			{
				NoOneInOldTeam = false; //all good exists someone in old team
				break;
			}
		if (NoOneInOldTeam)
		{
			m_TeamState[m_Core.Team(ClientID)] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(m_Core.Team(ClientID), false);
		}
	}

	if (Count(m_Core.Team(ClientID)) > 0)
		m_MembersCount[m_Core.Team(ClientID)]--;
}

int CGameTeams::Count(int Team) const
{
	if (Team == TEAM_SUPER)
		return -1;
	return m_MembersCount[Team];
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	int OldState = m_TeamState[Team];
	m_TeamState[Team] = State;
	onChangeTeamState(Team, State, OldState);
}

void CGameTeams::onChangeTeamState(int Team, int State, int OldState)
{
	if (OldState != State && State == TEAMSTATE_STARTED)
	{
		// OnTeamStateStarting
	}
	if (OldState != State && State == TEAMSTATE_FINISHED)
	{
		// OnTeamStateFinishing
	}
}

bool CGameTeams::TeamFinished(int Team)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (m_Core.Team(i) == Team && !m_TeeFinished[i])
			return false;
	return true;
}

int CGameTeams::TeamMask(int Team, int ExceptID, int Asker)
{
	if (Team == TEAM_SUPER)
		return -1;
	int Mask = 0;
	if (m_Core.GetSolo(Asker) && ExceptID == Asker)
		Mask = 0;
	if (m_Core.GetSolo(Asker))
		Mask = 1 << Asker;
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (i != ExceptID)
			if (((Character(i)
							&& ((!m_Core.GetSolo(i) && m_Core.Team(i) == Team)
									|| m_Core.Team(i) == TEAM_SUPER))
							|| ((GetPlayer(i) && (GetPlayer(i)->GetTeam() == -1 || GetPlayer(i)->m_Paused))
									&& (GetPlayer(i)->m_SpectatorID == SPEC_FREEVIEW
											|| m_Core.Team(GetPlayer(i)->m_SpectatorID) == Team))))
				Mask |= 1 << i;
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	CNetMsg_Cl_TeamsState Msg;
	Msg.m_Tee0 = m_Core.Team(0);
	Msg.m_Tee1 = m_Core.Team(1);
	Msg.m_Tee2 = m_Core.Team(2);
	Msg.m_Tee3 = m_Core.Team(3);
	Msg.m_Tee4 = m_Core.Team(4);
	Msg.m_Tee5 = m_Core.Team(5);
	Msg.m_Tee6 = m_Core.Team(6);
	Msg.m_Tee7 = m_Core.Team(7);
	Msg.m_Tee8 = m_Core.Team(8);
	Msg.m_Tee9 = m_Core.Team(9);
	Msg.m_Tee10 = m_Core.Team(10);
	Msg.m_Tee11 = m_Core.Team(11);
	Msg.m_Tee12 = m_Core.Team(12);
	Msg.m_Tee13 = m_Core.Team(13);
	Msg.m_Tee14 = m_Core.Team(14);
	Msg.m_Tee15 = m_Core.Team(15);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

int CGameTeams::GetDDRaceState(CPlayer* Player)
{
	if (!Player)
		return DDRACE_NONE;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_DDRaceState;
	return DDRACE_NONE;
}

void CGameTeams::SetDDRaceState(CPlayer* Player, int DDRaceState)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_DDRaceState = DDRaceState;
}

int CGameTeams::GetStartTime(CPlayer* Player)
{
	if (!Player)
		return 0;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_StartTime;
	return 0;
}

void CGameTeams::SetStartTime(CPlayer* Player, int StartTime)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_StartTime = StartTime;
}

void CGameTeams::SetCpActive(CPlayer* Player, int CpActive)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_CpActive = CpActive;
}

float *CGameTeams::GetCpCurrent(CPlayer* Player)
{
	if (!Player)
		return NULL;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_CpCurrent;
	return NULL;
}

void CGameTeams::OnTeamFinish(CPlayer** Players, unsigned int Size)
{
	float time = (float) (Server()->Tick() - GetStartTime(Players[0]))
			/ ((float) Server()->TickSpeed());
	if (time < 0.000001f)
		return;

	bool CallSaveScore = false;

#if defined(CONF_SQL)
	CallSaveScore = g_Config.m_SvUseSQL;
#endif

	int PlayerCIDs[MAX_CLIENTS];

	for(unsigned int i = 0; i < Size; i++)
	{
		PlayerCIDs[i] = Players[i]->GetCID();
	}

	if (CallSaveScore && Size >= 2)
		GameServer()->Score()->SaveTeamScore(PlayerCIDs, Size, time);
}

void CGameTeams::OnFinish(CPlayer* Player)
{
	if (!Player || !Player->IsPlaying())
		return;
	//TODO:DDRace:btd: this ugly
	float time = (float) (Server()->Tick() - GetStartTime(Player))
			/ ((float) Server()->TickSpeed());
	if (time < 0.000001f)
		return;
	CPlayerData *pData = GameServer()->Score()->PlayerData(Player->GetCID());
	char aBuf[128];
	SetCpActive(Player, -2);
	str_format(aBuf, sizeof(aBuf),
			"%s finished in: %d minute(s) %5.2f second(s)",
			Server()->ClientName(Player->GetCID()), (int) time / 60,
			time - ((int) time / 60 * 60));
	if (g_Config.m_SvHideScore)
		GameServer()->SendChatTarget(Player->GetCID(), aBuf);
	else
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	float diff = fabs(time - pData->m_BestTime);

	if (time - pData->m_BestTime < 0)
	{
		// new record \o/
		if (diff >= 60)
			str_format(aBuf, sizeof(aBuf), "New record: %d minute(s) %5.2f second(s) better.",
					(int) diff / 60, diff - ((int) diff / 60 * 60));
		else
			str_format(aBuf, sizeof(aBuf), "New record: %5.2f second(s) better.",
					diff);
		if (g_Config.m_SvHideScore)
			GameServer()->SendChatTarget(Player->GetCID(), aBuf);
		else
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
	else if (pData->m_BestTime != 0) // tee has already finished?
	{
		if (diff <= 0.005)
		{
			GameServer()->SendChatTarget(Player->GetCID(),
					"You finished with your best time.");
		}
		else
		{
			if (diff >= 60)
				str_format(aBuf, sizeof(aBuf), "%d minute(s) %5.2f second(s) worse, better luck next time.",
						(int) diff / 60, diff - ((int) diff / 60 * 60));
			else
				str_format(aBuf, sizeof(aBuf),
						"%5.2f second(s) worse, better luck next time.",
						diff);
			GameServer()->SendChatTarget(Player->GetCID(), aBuf); //this is private, sent only to the tee
		}
	}

	bool CallSaveScore = false;
#if defined(CONF_SQL)
	CallSaveScore = g_Config.m_SvUseSQL;
#endif

	if (!pData->m_BestTime || time < pData->m_BestTime)
	{
		// update the score
		pData->Set(time, GetCpCurrent(Player));
		CallSaveScore = true;
	}

	if (CallSaveScore)
		if (g_Config.m_SvNamelessScore || str_comp_num(Server()->ClientName(Player->GetCID()), "nameless tee",
				12) != 0)
			GameServer()->Score()->SaveScore(Player->GetCID(), time,
					GetCpCurrent(Player));

	bool NeedToSendNewRecord = false;
	// update server best time
	if (GameServer()->m_pController->m_CurrentRecord == 0
			|| time < GameServer()->m_pController->m_CurrentRecord)
	{
		// check for nameless
		if (g_Config.m_SvNamelessScore || str_comp_num(Server()->ClientName(Player->GetCID()), "nameless tee",
				12) != 0)
		{
			GameServer()->m_pController->m_CurrentRecord = time;
			//dbg_msg("character", "Finish");
			NeedToSendNewRecord = true;
		}
	}

	SetDDRaceState(Player, DDRACE_FINISHED);
	// set player score
	if (!pData->m_CurrentTime || pData->m_CurrentTime > time)
	{
		pData->m_CurrentTime = time;
		NeedToSendNewRecord = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (GetPlayer(i) && GetPlayer(i)->m_IsUsingDDRaceClient)
			{
				if (!g_Config.m_SvHideScore || i == Player->GetCID())
				{
					CNetMsg_Sv_PlayerTime Msg;
					Msg.m_Time = time * 100.0;
					Msg.m_ClientID = Player->GetCID();
					Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
				}
			}
		}
	}

	if (NeedToSendNewRecord && Player->m_IsUsingDDRaceClient)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (GameServer()->m_apPlayers[i]
					&& GameServer()->m_apPlayers[i]->m_IsUsingDDRaceClient)
			{
				GameServer()->SendRecord(i);
			}
		}
	}

	if (Player->m_IsUsingDDRaceClient)
	{
		CNetMsg_Sv_DDRaceTime Msg;
		Msg.m_Time = (int) (time * 100.0f);
		Msg.m_Check = 0;
		Msg.m_Finish = 1;

		if (pData->m_BestTime)
		{
			float Diff = (time - pData->m_BestTime) * 100;
			Msg.m_Check = (int) Diff;
		}

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, Player->GetCID());
	}

	int TTime = 0 - (int) time;
	if (Player->m_Score < TTime)
		Player->m_Score = TTime;

}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);

	if (!m_TeamLocked[m_Core.Team(ClientID)])
		SetForceCharacterTeam(ClientID, 0);
}

void CGameTeams::OnCharacterDeath(int ClientID)
{
	m_Core.SetSolo(ClientID, false);

	if (!m_TeamLocked[m_Core.Team(ClientID)])
		SetForceCharacterTeam(ClientID, 0);
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	m_TeamLocked[Team] = Lock;
}
