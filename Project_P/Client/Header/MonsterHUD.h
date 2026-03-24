#pragma once
#include "Component.h"

class CMonsterHUD final : public CComponent
{
	friend class CGameObject;

private:
	CMonsterHUD();
	~CMonsterHUD();

public:
	static CMonsterHUD* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;

public:
	void BIndMonster(class CMonster* _monster);

private:
	void CreateHPBar();
	void CreateSHBar();
	void CreateBABar();

public:
	CRectTransform* m_pRect;
	class CMonster* m_pMonster;
	CCanvas* m_pCanvas;
};

