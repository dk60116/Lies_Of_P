#pragma once

#include "epch.h"
#include "Component.h"

class CPlayerHUD final : public CComponent
{
public:
	struct PlayerHUDOptions
	{
		_int maxPotionStack = 8;
		_float gaugeFontWidth = 40.f;
		_float gaugeWidthMax = 700.f;
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
	
	struct DashAttackFrameSet
	{
		CImage* frame;
		CImage* usableGlow;
		CImage* cool;
		CImage* icon;
		CImage* blur_Circle;
		CImage* blur_Line;
		CImage* blur_Icon;

		_bool coolDown_Trigger = false;
		_float coolEndDuration = 0.f;
		_float coolEndDest = 0.5f;

		_bool prevReady = false;
		_bool glowAlbe = false;
		_float glowAlpa = 1.f;
		_float glowBlinkTime = 2.f;
	};

	struct SkillFrameSet
	{
		CImage* frame;
		CImage* icon;
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
	void CreateSkillFrame();

public:
	void Update_AllStatus(const CPlayer::PlayerStatus& _status);
	void Update_Potions(const _uint _maxValue, _uint _current);
	void Update_BE(const _uint _maxValue, const _uint _current);
	void Update_HP(const _uint _maxValue, const _uint _current);
	void Update_SH(const _uint _maxValue, const _uint _current);
	void Update_DashAttack(const _float _coolRatio, const _bool _ready);

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
	DashAttackFrameSet m_sDashAttackFrame;
	vector<SkillFrameSet> m_vSkillFrame;
};

