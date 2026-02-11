#include "cpch.h"
#include "GameManager.h"
#include "Player.h"

CGameManager::CGameManager()
	: m_strNextScene(L"")
	, m_pPlayer(nullptr)
	, m_pPlayerCamera(nullptr)
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
	m_pPlayer->AddRef();
}

void CGameManager::Remove_Player(CPlayer* _player)
{
	Safe_Release(_player);
	m_pPlayer = nullptr;
}

CPlayer* CGameManager::Get_Player()
{
	return m_pPlayer;
}

void CGameManager::Set_PlayerCamera(CPlayerCamera* _cam)
{
	m_pPlayerCamera = _cam;
}

CPlayerCamera* CGameManager::Get_PlayerCamera()
{
	return m_pPlayerCamera;
}

void CGameManager::Set_PlayerHUD(CPlayerHUD* _hud)
{
	m_pPlayerHUD = _hud;
}

CPlayerHUD* CGameManager::Get_PlayerHUD()
{
	return m_pPlayerHUD;
}

void CGameManager::Release()
{
	Safe_Release(m_pPlayer);
}
