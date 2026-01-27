#pragma once

#include "epch.h"

class CGameScene final : public CScene
{
public:
	CGameScene();
	~CGameScene();

public:
	HRESULT Initialize() override;
	void Awake() override;
	void Update() override;
	void SceneRelease() override;

private:
	class CPlayerCamera* m_pPlayerCamera;
	CLight* m_pDirLight;
	class CPlayer* m_pPlayer;

	class CPlayerHUD* m_pHUD;

	vector <class CMonster*> m_vMonsters;
};

