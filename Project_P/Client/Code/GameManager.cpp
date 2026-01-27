#include "cpch.h"
#include "GameManager.h"
#include "Player.h"

CGameManager::CGameManager()
	: m_strNextScene(L"")
	, m_pPlayer(nullptr)
	, m_pPlayerHUD(nullptr)
{
}

CGameManager::~CGameManager()
{
}

CGameManager& CGameManager::GetInstance()
{
	static CGameManager instance;
	return instance;
}

void CGameManager::Set_NexScene(const wstring _scneName)
{
	m_strNextScene = _scneName;
}

const wstring& CGameManager::Get_NextScene() const
{
	return m_strNextScene;
}

void CGameManager::Set_Player(CPlayer* _player)
{
	m_pPlayer = _player;
}

CPlayer* CGameManager::Get_Player()
{
	return m_pPlayer;
}

void CGameManager::Set_PlayerHUD(CPlayerHUD* _hud)
{
	m_pPlayerHUD = _hud;
}

CPlayerHUD* CGameManager::Get_PlayerHUD()
{
	return m_pPlayerHUD;
}
