#pragma once
#include "Component.h"

class CMonsterHUD final : public CComponent
{
	friend class CGameObject;

public:
	struct MonsterHUDOPtions
	{
		_float rectSize = 4.f;
	};

	struct HPGaugeSet
	{
		CRectTransform* rect;
		vector<CImage*> bg;
		vector<CImage*> fill;
	};

	struct SEGaugeSet
	{
		CRectTransform* rect;
		vector<CImage*> bg;
		vector<CImage*> fill;
	};

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
	void SetHUDVisible(const _bool _visible);

private:
	void CreateHPBar();
	void CreateSHBar();
	void CreateBABar();
	void UpdateRectWidthFromMaxHP();
	void Update_HP(const _int _maxValue, const _int _current);
	void Update_SH(const _int _maxValue, const _int _current);
	void Update_BA(const _int _maxValue, const _int _current);

public:
	CRectTransform* m_pRect;
	class CMonster* m_pMonster;
	CCanvas* m_pCanvas;

private:
	MonsterHUDOPtions m_sOption;

	CRectTransform* m_pHPBarRect;
	CRectTransform* m_pSHBarRect;
	CRectTransform* m_pBABarRect;
	vector<HPGaugeSet> m_vHPBox;
	vector<SEGaugeSet> m_vSHBox;
	vector<SEGaugeSet> m_vBABox;
};

