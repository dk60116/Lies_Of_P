#pragma once

#include "epch.h"
#include "Player.h"
#include "PlayerCamera.h"
#include "PlayerHUD.h"
#include "Monster.h"
#include "MonsterController.h"

class CGameManager final
{
	SINGLETONCLASS(CGameManager);

public:
	void Set_NexScene(const wstring _scneName);
	const wstring& Get_NextScene() const;

public:
	void Set_Player(CPlayer* _player);
	void Remove_Player(CPlayer* _player);
	CPlayer* Get_Player();

	void Set_PlayerCamera(CPlayerCamera* _cam);
	CPlayerCamera* Get_PlayerCamera();

	void Set_PlayerHUD(CPlayerHUD* _hud);
	CPlayerHUD* Get_PlayerHUD();

	void Release();

private:
	wstring m_strNextScene;
	CPlayer* m_pPlayer;
	CPlayerCamera* m_pPlayerCamera;
	CPlayerHUD* m_pPlayerHUD;
};

