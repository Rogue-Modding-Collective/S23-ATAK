// Read-only client data. No entity/component references survive a refresh.
enum ATAK_ESquadStatus
{
	UNAVAILABLE,
	NO_SQUAD,
	READY
}

class ATAK_SquadMember
{
	int m_iPlayerId;
	string m_sDisplayName;
	bool m_bLeader;
	bool m_bLocal;
}

class ATAK_SquadData
{
	ATAK_ESquadStatus m_eStatus = ATAK_ESquadStatus.UNAVAILABLE;
	int m_iGroupId = -1;
	int m_iLeaderId = -1;
	string m_sDisplayName;
	ref array<ref ATAK_SquadMember> m_Members = {};

	string Identity()
	{
		if (m_eStatus == ATAK_ESquadStatus.NO_SQUAD) return "No squad";
		if (m_eStatus != ATAK_ESquadStatus.READY) return "Squad information unavailable";
		return m_sDisplayName;
	}

	void Refresh(bool includeMembers)
	{
		m_eStatus = ATAK_ESquadStatus.UNAVAILABLE;
		m_iGroupId = -1;
		m_iLeaderId = -1;
		m_sDisplayName = string.Empty;
		m_Members.Clear();
		if (!GetGame() || System.IsConsoleApp()) return;
		int localId = SCR_PlayerController.GetLocalPlayerId();
		PlayerManager players = GetGame().GetPlayerManager();
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (localId <= 0 || !players || !groups) return;
		SCR_AIGroup group = groups.GetPlayerGroup(localId);
		if (!group)
		{
			// A known membership whose entity has not arrived is not "No squad".
			SCR_PlayerControllerGroupComponent membership = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
			if (membership && membership.GetGroupID() >= 0) return;
			m_eStatus = ATAK_ESquadStatus.NO_SQUAD;
			return;
		}
		m_eStatus = ATAK_ESquadStatus.READY;
		m_iGroupId = group.GetGroupID();
		m_iLeaderId = group.GetLeaderID();
		m_sDisplayName = SCR_GroupHelperUI.GetTranslatedGroupName(group);
		if (m_sDisplayName.IsEmpty()) m_sDisplayName = string.Format("Squad %1", m_iGroupId);
		if (!includeMembers) return;
		array<int> ids = group.GetPlayerIDs();
		if (!ids) return;
		// Copy before sorting: GetPlayerIDs returns the group's live array.
		array<int> ordered = {};
		foreach (int memberId : ids)
		{
			if (memberId > 0 && players.IsPlayerConnected(memberId) && !ordered.Contains(memberId))
				ordered.Insert(memberId);
		}
		ordered.Sort();
		if (ordered.Contains(m_iLeaderId))
		{
			ordered.RemoveItem(m_iLeaderId);
			ordered.InsertAt(m_iLeaderId, 0);
		}
		foreach (int playerId : ordered)
		{
			ATAK_SquadMember member = new ATAK_SquadMember();
			member.m_iPlayerId = playerId;
			member.m_sDisplayName = players.GetPlayerName(playerId);
			if (member.m_sDisplayName.IsEmpty()) member.m_sDisplayName = string.Format("Player %1", playerId);
			member.m_bLeader = playerId == m_iLeaderId;
			member.m_bLocal = playerId == localId;
			m_Members.Insert(member);
		}
	}
}
