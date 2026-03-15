#pragma once

#include "Component.h"

class CPlayerHUD final : public CComponent
{
public:
	struct PlayerHUDOptions
	{
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

private:
	CPlayer* m_pPlayer;
	CCanvas* m_pCanvas;
	PlayerHUDOptions m_sOptions;

private:
	CImage* m_pAimImage;
	CImage* m_pPotionHolderImage;
	CImage* m_pPCBtnImage;
	CText* m_pTextPotionCount;
	vector<CText*> m_vGaugeTexts;
};

