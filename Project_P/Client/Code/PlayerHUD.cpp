#include "cpch.h"
#include "PlayerHUD.h"

namespace
{
	enum GaugeBatchGroupId : _int
	{
		BatchGroup_HP_BG = 200,
		BatchGroup_HP_FILL,
		BatchGroup_SH_BG,
		BatchGroup_SH_FILL,
		BatchGroup_BA_BG,
		BatchGroup_BA_FILL
	};
}

CPlayerHUD::CPlayerHUD()
	: m_pPlayer(nullptr)
	, m_pCanvas(nullptr)
	, m_sOptions({})
	, m_sCachedStatus({})
	, m_fDisplayedHp(0.f)
	, m_bHpDisplayInitialized(false)
	, m_pAimImage(nullptr)
	, m_pPotionHolderImage(nullptr)
	, m_vPotionStacks({})
	, m_pPCBtnImage(nullptr)
	, m_pTextPotionCount(nullptr)
	, m_vGaugeTexts({})
	, m_vBEBox({})
	, m_vHPBox({})
	, m_vSHBox({})
	, m_sDashAttackFrame({})
	, m_vSkillFrame({})
{
}

CPlayerHUD::~CPlayerHUD()
{
}

CPlayerHUD* CPlayerHUD::Create()
{
	return new CPlayerHUD();
}

CComponent* CPlayerHUD::Clone() const
{
	return new CPlayerHUD();
}

HRESULT CPlayerHUD::Initialize()
{
	m_pCanvas = m_pGameObject->AddComponent<CCanvas>();

	CreateAim();
	CreatePotions();
	CreateGauge();
	CreateSkillFrame();

	return S_OK;
}

void CPlayerHUD::Awake()
{
	m_pPlayer = CGameManager::GetInstance().Get_Player();
	CGameManager::GetInstance().Set_PlayerHUD(this);
}

void CPlayerHUD::Start()
{
	Update_AllStatus(m_pPlayer->Get_PlayerEquipStat());
}

void CPlayerHUD::Update()
{
	if (!m_bHpDisplayInitialized)
		return;

	Update_DashAttack(m_pPlayer->GetDashAttackCooldownRatio(), m_pPlayer->IsDashAttackReady());

	const _float targetHp = static_cast<_float>(m_sCachedStatus.crtHp);

	const _float followSpeed = 10.f;
	const _float t = 1.f - std::exp(-followSpeed * DELTA_TIME);

	m_fDisplayedHp += (targetHp - m_fDisplayedHp) * t;

	if (fabsf(targetHp - m_fDisplayedHp) < 0.05f)
		m_fDisplayedHp = targetHp;

	Update_HP(m_sCachedStatus.maxHp, static_cast<_uint>(std::round(m_fDisplayedHp)));
}

void CPlayerHUD::OnDestroy()
{
	if (CGameManager::GetInstance().Get_PlayerHUD() == this)
		CGameManager::GetInstance().Set_PlayerHUD(nullptr);
}

void CPlayerHUD::CreateAim()
{
	CGameObject* aimImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Aim");
	m_pAimImage = aimImageObj->AddComponent<CImage>();
	aimImageObj->GetTransform()->SetParent(GetTransform());

	m_pAimImage->GetRectTransform()->Set_WidthHeight(3, 3);
	m_pAimImage->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Aim.png"));
}

void CPlayerHUD::CreatePotions()
{
	CGameObject* potionHolderObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionHolder");
	m_pPotionHolderImage = potionHolderObj->AddComponent<CImage>();
	potionHolderObj->GetTransform()->SetParent(GetTransform());

	m_pPotionHolderImage->GetRectTransform()->Set_WidthHeight(90, 90);
	m_pPotionHolderImage->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Potion_Holder.png"));

	m_pPotionHolderImage->GetRectTransform()->Set_Pivot(0.f, 0.f);
	m_pPotionHolderImage->GetRectTransform()->Set_AnchorsMin(0.f, 0.f);
	m_pPotionHolderImage->GetRectTransform()->Set_AnchoredPosition(20.f, 140.f);

	CGameObject* pcBtnObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PCBtn");
	m_pPCBtnImage = pcBtnObj->AddComponent<CImage>();
	pcBtnObj->GetTransform()->SetParent(GetTransform());

	m_pPCBtnImage->GetRectTransform()->Set_WidthHeight(90, 90);
	m_pPCBtnImage->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Btn_PcEmpty.png"));

	m_pPCBtnImage->GetRectTransform()->Set_Pivot(0.f, 0.f);
	m_pPCBtnImage->GetRectTransform()->Set_AnchorsMin(0.f, 0.f);
	m_pPCBtnImage->GetRectTransform()->Set_AnchoredPosition(20.f, 80.f);

	CGameObject* potionCountObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionCount");
	m_pTextPotionCount = potionCountObj->AddComponent<CText>();
	potionCountObj->GetTransform()->SetParent(m_pPotionHolderImage->GetRectTransform());
	m_pTextPotionCount->SetFont(CResources::GetInstance().LoadOnGame<CFont>(L"Orbitron Medium (Font)"));
	m_pTextPotionCount->SetFontSize(5.f);
	m_pTextPotionCount->SetColor(ColorValue::white());
	m_pTextPotionCount->SetText(L"3");
	m_pTextPotionCount->GetRectTransform()->Set_AnchorsMin(1.f, 0.f);
	m_pTextPotionCount->GetRectTransform()->Set_Pivot(0.f, 0.f);
	m_pTextPotionCount->GetRectTransform()->Set_AnchoredPosition(-20.f, 20.f);
	m_pTextPotionCount->GetRectTransform()->Set_AnchoredSize(30.f, 30.f);
	m_pTextPotionCount->GetRectTransform()->Set_LocalScaleY(2.5f);
	m_pTextPotionCount->SetFontSize(2.5f);
	m_pTextPotionCount->SetAligmentHorizontal(CText::TextAligmentHorizontal::Left);
	m_pTextPotionCount->SetAligmentVertical(CText::TexAligmentVertical::Bottom);

	CGameObject* stackRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionStackRect");
	CRectTransform* stackRect = stackRectObj->AddComponent<CRectTransform>();
	stackRect->SetParent(potionHolderObj->GetTransform());
	stackRect->Set_AnchorsMin(0.5f, 0.f);
	stackRect->Set_PivotY(0.f);
	stackRect->Set_AnchoredPositionY(26.f);
	stackRect->Set_WidthHeight(27.f, 43.f);

	for (_int i = 0; i < m_sOptions.maxPotionStack; ++i)
	{
		CGameObject* stackObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionStack_" + to_wstring(i));
		m_vPotionStacks.push_back(stackObj->AddComponent<CImage>());
		m_vPotionStacks.back()->GetRectTransform()->SetParent(stackRect);
		m_vPotionStacks.back()->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_potion_stack.png"));
	}

	CVerticalLayoutGroup* vlg =  stackRect->Get_GameObject()->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, true);
	vlg->SetSpacing(2.f);
}

void CPlayerHUD::CreateGauge()
{
	CGameObject* gaugeObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauges");
	CRectTransform* rect = gaugeObj->AddComponent<CRectTransform>();
	gaugeObj->GetTransform()->SetParent(GetTransform());
	rect->Set_Pivot(0.f, 0.f);
	rect->Set_AnchorsMin(0.f, 0.f);
	rect->Set_AnchoredPosition(110.f, 55.f);
	rect->Set_AnchoredSize(m_sOptions.gaugeWidthMax, 73.f);

	CGameObject* beObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_BE");
	CRectTransform* rect_be = beObj->AddComponent<CRectTransform>();
	beObj->GetTransform()->SetParent(rect);
	CRectTransform* beGR = CreateGauge_Default(L"BE", rect_be);
	CreateGauge_BE(beGR);
	rect_be->Set_Height(20.f);

	CGameObject* hpObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_HP");
	CRectTransform* rect_hp = hpObj->AddComponent<CRectTransform>();
	rect_hp->GetTransform()->SetParent(rect);
	CRectTransform* hpGR = CreateGauge_Default(L"HP", rect_hp);
	CreateGauge_HP(hpGR);
	rect_hp->Set_Height(25.f);

	CGameObject* shObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_SH");
	CRectTransform* rect_sh = shObj->AddComponent<CRectTransform>();
	rect_sh->GetTransform()->SetParent(rect);
	CRectTransform* shGR = CreateGauge_Default(L"SH", rect_sh);
	CreateGauge_SH(shGR);
	rect_sh->Set_Height(20.f);

	CVerticalLayoutGroup* vlg = rect->Get_GameObject()->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, false);
	vlg->SetSpacing(4.f);
}

CRectTransform* CPlayerHUD::CreateGauge_Default(const wstring& _name, CRectTransform* _parent)
{
	CGameObject* gaugeTextObj = m_pGameObject->Get_Scene()->Add_GameObject(_name + L"_Text");
	m_vGaugeTexts.push_back(gaugeTextObj->AddComponent<CText>());
	gaugeTextObj->GetTransform()->SetParent(_parent);
	m_vGaugeTexts.back()->SetFont(CResources::GetInstance().LoadOnGame<CFont>(L"Orbitron Medium (Font)"));
	m_vGaugeTexts.back()->SetColor(ColorValue::white());
	m_vGaugeTexts.back()->SetFontSize(4.f);
	m_vGaugeTexts.back()->SetText(_name);
	m_vGaugeTexts.back()->SetAligmentHorizontal(CText::TextAligmentHorizontal::Left);
	m_vGaugeTexts.back()->GetRectTransform()->Set_Width(40.f);

	CGameObject* gaugeRect = m_pGameObject->Get_Scene()->Add_GameObject(_name + L"_Gauge");
	CRectTransform* gr = gaugeRect->AddComponent<CRectTransform>();
	gr->SetParent(_parent);
	gr->Set_Width(m_sOptions.gaugeWidthMax - m_sOptions.gaugeFontWidth);

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);

	return gr;
}

void CPlayerHUD::CreateGauge_BE(CRectTransform* _parent)
{
	_parent->Set_PivotX(0.f);

	CTexture* emptyTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Beta_cube_00.png");
	CTexture* fullTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Beta_cube_05.png");

	for (_int i = 0; i < 14; ++i)
	{
		CGameObject* gaugeImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BE_GaugeImage_Empty_" + to_wstring(i));
		CImage* empty = gaugeImageObj->AddComponent<CImage>();
		gaugeImageObj->GetTransform()->SetParent(_parent);
		empty->SetTexture(emptyTex);
		empty->GetRectTransform()->Set_WidthHeight(14.f, 14.f);

		CGameObject* fullImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BE_GaugeImage_Full_" + to_wstring(i));
		CImage* full = fullImageObj->AddComponent<CImage>();
		fullImageObj->GetTransform()->SetParent(gaugeImageObj->GetTransform());
		full->SetTexture(fullTex);
		full->Set_FillMethod(CImage::FillMethod::Horizontal);
		full->SetColor(ColorValue(204, 233, 244));
		full->GetRectTransform()->Set_AnchoredSize(empty->GetRectTransform()->Get_WidthHeight());
		
		m_vBEBox.push_back({ empty, full });
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	const CLayoutGroup::Padding p = { 0.f, 0.f, 3.f, 3.f };
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetPadding(p);
	hlg->SetSpacing(4.f);
}

void CPlayerHUD::CreateGauge_HP(CRectTransform* _parent)
{
	CTexture* emptyTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Hp_Dot1_5x_bg.png");
	CTexture* fullTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Hp_Dot1_5x.png");

	const _float rectSize = 7.f;

	for (_int i = 0; i < 65; ++i)
	{
		CGameObject* hpRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_" + to_wstring(i));
		CRectTransform* rect = hpRectObj->AddComponent<CRectTransform>();
		rect->SetParent(_parent);
		rect->Set_Width(rectSize);

		HPGaugeSet set = {};
		set.rect = rect;

		for (_int j = 0; j < 3; ++j)
		{
			CGameObject* hpEmptyObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_Empty_" + to_wstring(j));
			CImage* hpRect = hpEmptyObj->AddComponent<CImage>();
			hpRect->GetTransform()->SetParent(rect);
			hpRect->SetTexture(emptyTex);
			hpRect->SetGroupID(BatchGroup_HP_BG);
			hpRect->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
			set.bg.push_back(hpRect);

			CGameObject* hpFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_Dot_" + to_wstring(j));
			CImage* hpFullRect = hpFullObj->AddComponent<CImage>();
			hpFullRect->GetTransform()->SetParent(hpRect->GetRectTransform());
			hpFullRect->SetTexture(fullTex);
			hpFullRect->SetGroupID(BatchGroup_HP_FILL);
			hpFullRect->Set_FillMethod(CImage::FillMethod::Horizontal);
			hpFullRect->GetRectTransform()->Set_WidthHeight(hpRect->GetRectTransform()->Get_WidthHeight());
			set.fill.push_back(hpFullRect);
		}

		m_vHPBox.push_back(set);

		CVerticalLayoutGroup* vlg = hpRectObj->AddComponent<CVerticalLayoutGroup>();
		vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		vlg->SetSpacing(0.f);
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetSpacing(0.f);
	hlg->SetControlChildSize(false, true);
}

void CPlayerHUD::CreateGauge_SH(CRectTransform* _parent)
{
	CTexture* emptyTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Shield_Dot1_5x_Bg.png");
	CTexture* fullTex = CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_Shield_Dot1_5x.png");

	const _float rectSize = 7.f;
	const _int rectCount = 11;
	const _float boxSize = rectSize * rectCount;

	for (_int i = 0; i < 5; i++)
	{
		CGameObject* boxObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Box_" + to_wstring(i));
		CRectTransform* boxRect = boxObj->AddComponent<CRectTransform>();
		boxRect->SetParent(_parent);
		boxRect->Set_Width(boxSize);

		for (_int j = 0; j < rectCount; ++j)
		{
			CGameObject* hpRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Rect_" + to_wstring(j));
			CRectTransform* rect = hpRectObj->AddComponent<CRectTransform>();
			rect->SetParent(boxRect);
			rect->Set_WidthHeight(rectSize, rectSize * 2.f);

			SEGaugeSet set = {};
			set.rect = rect;

			for (_int k = 0; k < 2; ++k)
			{
				CGameObject* hpEmptyObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Rect_Empty_" + to_wstring(k));
				CImage* hpRect = hpEmptyObj->AddComponent<CImage>();
				hpRect->GetTransform()->SetParent(rect);
				hpRect->SetTexture(emptyTex);
				hpRect->SetGroupID(BatchGroup_SH_BG);
				hpRect->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
				set.bg.push_back(hpRect);

				CGameObject* hpFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Rect_Dot_" + to_wstring(k));
				CImage * hpFullRect = hpFullObj->AddComponent<CImage>();
				hpFullRect->GetTransform()->SetParent(hpRect->GetRectTransform());
				hpFullRect->SetTexture(fullTex);
				hpFullRect->SetGroupID(BatchGroup_SH_FILL);
				hpFullRect->SetColor(ColorValue(173, 209, 196));
				hpFullRect->Set_FillMethod(CImage::FillMethod::Horizontal);
				hpFullRect->GetRectTransform()->Set_WidthHeight(hpRect->GetRectTransform()->Get_WidthHeight());
				set.fill.push_back(hpFullRect);
			}

			m_vSHBox.push_back(set);

			CVerticalLayoutGroup* vlg = hpRectObj->AddComponent<CVerticalLayoutGroup>();
			vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		}

		CHorizontalLayoutGroup* hlg = boxRect->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
		hlg->SetControlChildSize(false, false);
		hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetControlChildSize(false, true);
	hlg->SetSpacing(rectSize + 0.5f);
}

void CPlayerHUD::CreateSkillFrame()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Skill_Rect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(GetTransform());
	rect->Set_AnchorsMin(0.f, 0.f);
	rect->Set_AnchorsMax(1.f, 0.f);
	rect->Set_Pivot(1.f, 0.f);
	rect->Set_AnchoredPosition(-50.f, 50.f);
	rect->Set_WidthHeight(180);

	{
		CGameObject* dashAttackFrameObj = m_pGameObject->Get_Scene()->Add_GameObject(L"DashAttack Frame");
		CImage* imgFrame = dashAttackFrameObj->AddComponent<CImage>();
		imgFrame->GetRectTransform()->SetParent(rect);
		imgFrame->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_skill_hud_dashframe.png"));
		imgFrame->GetRectTransform()->Set_PivotY(0.f);
		imgFrame->GetRectTransform()->Set_AnchoredPositionY(110.f);
		imgFrame->GetRectTransform()->Set_WidthHeight(50);

		CGameObject* dashAttadckGlowObj = m_pGameObject->Get_Scene()->Add_GameObject(L"DashAttack Glow");
		CImage* imgGlow = dashAttadckGlowObj->AddComponent<CImage>();
		imgGlow->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgGlow->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgGlow->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_cicle_glow.png"));

		CGameObject* dashAttadckCoolObj = m_pGameObject->Get_Scene()->Add_GameObject(L"DashAttack Cool");
		CImage* imgCool = dashAttadckCoolObj->AddComponent<CImage>();
		imgCool->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgCool->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgCool->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_skill_hud_dashframe_Cool.png"));
		imgCool->Set_FillMethod(CImage::FillMethod::Radial360);
		imgCool->Set_FillOrigin((_int)CImage::Radial360_FillOrigin::Top);
		imgCool->Set_FillClockwise(false);

		CGameObject* dashAttadckIconObj = m_pGameObject->Get_Scene()->Add_GameObject(L"DashAttack Icon");
		CImage* imgIcon = dashAttadckIconObj->AddComponent<CImage>();
		imgIcon->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgIcon->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgIcon->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/SI_DashAttack1.png"));

		CGameObject* blur_CirleObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Blur_Clrcle");
		CImage* imgBlur_Circle = blur_CirleObj->AddComponent<CImage>();
		imgBlur_Circle->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgBlur_Circle->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgBlur_Circle->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_blur_circle.png"));
		imgBlur_Circle->SetColor(ColorValue(29, 101, 255));

		CGameObject* blur_LineObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Blur_Line");
		CImage* imgBlur_Line = blur_LineObj->AddComponent<CImage>();
		imgBlur_Line->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgBlur_Line->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgBlur_Line->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_blur_circle_line.png"));
		imgBlur_Line->SetColor(ColorValue(29, 101, 255));
		imgBlur_Line->SetAlpha(0.5f);


		CGameObject* blur_IocnObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Blur_Icon");
		CImage* imgBlur_Icon = blur_IocnObj->AddComponent<CImage>();
		imgBlur_Icon->GetRectTransform()->SetParent(imgFrame->GetTransform());
		imgBlur_Icon->GetRectTransform()->Set_WidthHeight(imgFrame->GetRectTransform()->Get_WidthHeight());
		imgBlur_Icon->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/SI_DashAttack1_glow.png"));
		
		m_sDashAttackFrame = { imgFrame, imgGlow, imgCool, imgIcon, imgBlur_Circle, imgBlur_Line, imgBlur_Icon };
	}

	{
		for (_int i = 0; i < 4; ++i)
		{
			CGameObject* skillFrameObject = m_pGameObject->Get_Scene()->Add_GameObject(L"SkillFrame_" + to_wstring(i));
			CImage* skillFrameImg = skillFrameObject->AddComponent<CImage>();
			skillFrameImg->GetRectTransform()->SetParent(rect);
			skillFrameImg->SetTexture(CResources::GetInstance().LoadOnPath<CTexture>(L"UI/HUDUI/HUD_skill_hud_frame.png"));

			m_vSkillFrame.push_back({ skillFrameImg, nullptr });
		}

		m_vSkillFrame[0].frame->GetRectTransform()->Set_PivotY(0.f);
		m_vSkillFrame[1].frame->GetRectTransform()->Set_PivotX(1.f);
		m_vSkillFrame[2].frame->GetRectTransform()->Set_PivotX(0.f);
		m_vSkillFrame[3].frame->GetRectTransform()->Set_PivotY(1.f);

		m_vSkillFrame[0].frame->GetRectTransform()->Set_AnchoredPositionY(5.f);
		m_vSkillFrame[1].frame->GetRectTransform()->Set_AnchoredPositionX(-5.f);
		m_vSkillFrame[2].frame->GetRectTransform()->Set_AnchoredPositionX(5.f);
		m_vSkillFrame[3].frame->GetRectTransform()->Set_AnchoredPositionY(-5.f);
	}
}

void CPlayerHUD::Update_AllStatus(const CPlayer::PlayerStatus& _status)
{
	m_sCachedStatus = _status;

	if (!m_bHpDisplayInitialized)
	{
		m_fDisplayedHp = static_cast<_float>(_status.crtHp);
		m_bHpDisplayInitialized = true;
	}

	Update_Potions(_status.maxPotion, _status.crtPotion);
	Update_BE(_status.maxBetaEnergy, _status.crtBeatEnergy);
	Update_SH(_status.maxShield, _status.crtShield);
	Update_HP(_status.maxHp, static_cast<_uint>(std::round(m_fDisplayedHp)));
}

void CPlayerHUD::Update_Potions(const _uint _maxValue, _uint _current)
{
	for (size_t i = 0; i < m_vPotionStacks.size(); ++i)
	{
		m_vPotionStacks[i]->Get_GameObject()->SetActive(i < _maxValue);
	}
}

void CPlayerHUD::Update_BE(const _uint _maxValue, const _uint _current)
{
	for (size_t i = 0; i < m_vBEBox.size(); ++i)
	{
		m_vBEBox[i].bg->Get_GameObject()->SetActive(i < _maxValue / 200);
	}
}

void CPlayerHUD::Update_HP(const _uint _maxValue, const _uint _current)
{
	const _float rectCapacity = 100.f;

	for (size_t i = 0; i < m_vHPBox.size(); ++i)
	{
		HPGaugeSet& set = m_vHPBox[i];
		const _float rectStart = static_cast<_float>(i) * rectCapacity;
		const _float rectMax = std::clamp(static_cast<_float>(_maxValue) - rectStart, 0.f, rectCapacity);
		const _float rectCurrent = std::clamp(static_cast<_float>(_current) - rectStart, 0.f, rectCapacity);
		const _bool rectActive = rectMax > 0.f;
		const _float rectFillAmount = (rectCapacity > 0.f) ? (rectCurrent / rectCapacity) : 0.f;

		set.rect->Get_GameObject()->SetActive(rectActive);
		if (!rectActive || set.fill.empty())
			continue;

		for (CImage* fillImage : set.fill)
		{
			if (!fillImage)
				continue;

			fillImage->SetFillAmount(rectFillAmount);
			fillImage->Get_GameObject()->SetActive(rectFillAmount > 0.f);
		}
	}
}

void CPlayerHUD::Update_SH(const _uint _maxValue, const _uint _current)
{
	for (size_t i = 0; i < m_vSHBox.size(); ++i)
	{
		m_vSHBox[i].rect->Get_GameObject()->SetActive(i < _maxValue / (500.f / 22.f));
	}
}

void CPlayerHUD::Update_DashAttack(const _float _coolRatio, const _bool _ready)
{
	if (!m_sDashAttackFrame.prevReady && _ready)
	{
		m_sDashAttackFrame.coolEndDuration = 0.f;
		m_sDashAttackFrame.blur_Line->Get_GameObject()->SetActive(true);
		m_sDashAttackFrame.blur_Circle->Get_GameObject()->SetActive(true);
		m_sDashAttackFrame.blur_Circle->SetAlpha(1.f);
		m_sDashAttackFrame.coolDown_Trigger = true;
		m_sDashAttackFrame.blur_Icon->Get_GameObject()->SetActive(true);
		m_sDashAttackFrame.blur_Icon->SetAlpha(1.f);
	}

	if (m_sDashAttackFrame.coolDown_Trigger)
	{
		if (m_sDashAttackFrame.coolEndDuration <= m_sDashAttackFrame.coolEndDest * 0.5f)
			m_sDashAttackFrame.blur_Line->GetRectTransform()->Set_WidthHeight(static_cast<_int>(300.f * (m_sDashAttackFrame.coolEndDuration / m_sDashAttackFrame.coolEndDest)));

		m_sDashAttackFrame.coolEndDuration += DELTA_TIME;

		if (m_sDashAttackFrame.coolEndDuration <= m_sDashAttackFrame.coolEndDest)
		{
			const _float fade = 1.f - (m_sDashAttackFrame.coolEndDuration / m_sDashAttackFrame.coolEndDest);

			m_sDashAttackFrame.blur_Circle->SetAlpha(fade);
			m_sDashAttackFrame.blur_Circle->GetRectTransform()->Set_WidthHeight(static_cast<_int>(150.f * (1.f - (m_sDashAttackFrame.coolEndDuration / m_sDashAttackFrame.coolEndDest))));
			m_sDashAttackFrame.blur_Icon->SetAlpha(fade);
		}

		if (m_sDashAttackFrame.coolEndDuration > m_sDashAttackFrame.coolEndDest * 0.5f)
		{
			m_sDashAttackFrame.blur_Line->Get_GameObject()->SetActive(false);
		}

		if (m_sDashAttackFrame.coolEndDuration >= m_sDashAttackFrame.coolEndDest)
		{
			m_sDashAttackFrame.blur_Circle->Get_GameObject()->SetActive(false);
			m_sDashAttackFrame.coolDown_Trigger = false;
			m_sDashAttackFrame.blur_Icon->Get_GameObject()->SetActive(false);
		}
	}

	m_sDashAttackFrame.prevReady = _ready;

	m_sDashAttackFrame.usableGlow->Get_GameObject()->SetActive(_ready);
	m_sDashAttackFrame.cool->SetFillAmount(_coolRatio);
	m_sDashAttackFrame.cool->Get_GameObject()->SetActive(!_ready);

	if (_ready)
	{
		m_sDashAttackFrame.usableGlow->SetAlpha(m_sDashAttackFrame.glowAlpa);

		if (!m_sDashAttackFrame.glowAlbe)
		{
			m_sDashAttackFrame.glowAlpa -= DELTA_TIME * (1.f / m_sDashAttackFrame.glowBlinkTime);

			if (m_sDashAttackFrame.glowAlpa <= 0.f)
			{
				m_sDashAttackFrame.glowAlpa = 0.f;
				m_sDashAttackFrame.glowAlbe = true;
			}
		}
		else
		{
			m_sDashAttackFrame.glowAlpa += DELTA_TIME * (1.f / m_sDashAttackFrame.glowBlinkTime);

			if (m_sDashAttackFrame.glowAlpa >= 1.f)
			{
				m_sDashAttackFrame.glowAlpa = 1.f;
				m_sDashAttackFrame.glowAlbe = false;
			}
		}
	}
}
