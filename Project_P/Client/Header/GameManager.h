#pragma once

#include "epch.h"
#include "Player.h"
#include "PlayerHUD.h"

class CGameManager final
{
	SINGLETONCLASS(CGameManager);

public:
	void Set_NexScene(const wstring _scneName);
	const wstring& Get_NextScene() const;

public:
	void Set_Player(CPlayer* _player);
	CPlayer* Get_Player();

	void Set_PlayerHUD(CPlayerHUD* _hud);
	CPlayerHUD* Get_PlayerHUD();

private:
	wstring m_strNextScene;
	CPlayer* m_pPlayer;
	CPlayerHUD* m_pPlayerHUD;
};

