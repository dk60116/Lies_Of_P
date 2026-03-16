#pragma once

#include "Component.h"

class CPlayerHUD final : public CComponent
{
public:
	struct PlayerHUDOptions
	{
		_int maxPotionStack = 8;
		_float gaugeFontWidth = 40.f;
		_float gaugeWidthMax = 1000.f;
	};

	struct BEGaugeSet
	{
		CImage* bg;
		CImage* fill;
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
	CPlayerHUD();
	~CPlayerHUD();

public:
	static CPlayerHUD* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;

private:
	void CreateAim();
	void CreatePotions();
	void CreateGauge();
	CRectTransform* CreateGauge_Default(const wstring& _name, CRectTransform* _parent);
	void CreateGauge_BE(CRectTransform* _parent);
	void CreateGauge_HP(CRectTransform* _parent);
	void CreateGauge_SH(CRectTransform* _parent);

public:
	void Update_Status(const CPlayer::PlayerStatus& _status);
	void Update_BE(const _uint _maxValue, const _uint _current);
	void Update_HP(const _uint _maxValue, const _uint _current);
	void Update_SH(const _uint _maxValue, const _uint _current);

private:
	CPlayer* m_pPlayer;
	CCanvas* m_pCanvas;
	PlayerHUDOptions m_sOptions;
	CPlayer::PlayerStatus m_sCachedStatus;
	_float m_fDisplayedHp;
	_bool m_bHpDisplayInitialized;

private:
	CImage* m_pAimImage;
	CImage* m_pPotionHolderImage;
	vector<CImage*> m_vPotionStacks;
	CImage* m_pPCBtnImage;
	CText* m_pTextPotionCount;
	vector<CText*> m_vGaugeTexts;
	vector<BEGaugeSet> m_vBEBox;
	vector<HPGaugeSet> m_vHPBox;
	vector<SEGaugeSet> m_vSHBox;
};

