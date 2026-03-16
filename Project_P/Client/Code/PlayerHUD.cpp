#include "cpch.h"
#include "PlayerHUD.h"

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

	return S_OK;
}

void CPlayerHUD::Awake()
{
	m_pPlayer = CGameManager::GetInstance().Get_Player();
	CGameManager::GetInstance().Set_PlayerHUD(this);
}

void CPlayerHUD::Start()
{
	Update_Status(m_pPlayer->Get_PlayerEquipStat());
}

void CPlayerHUD::Update()
{
	if (!m_bHpDisplayInitialized)
		return;

	const _float targetHp = static_cast<_float>(m_sCachedStatus.crtHp);
	const _float dt = std::clamp(DELTA_TIME, 0.f, 0.05f);
	const _float followSpeed = 10.f;
	const _float t = 1.f - std::exp(-followSpeed * dt);

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
	m_pAimImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Aim (Texture)"));
}

void CPlayerHUD::CreatePotions()
{
	CGameObject* potionHolderObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionHolder");
	m_pPotionHolderImage = potionHolderObj->AddComponent<CImage>();
	potionHolderObj->GetTransform()->SetParent(GetTransform());

	m_pPotionHolderImage->GetRectTransform()->Set_WidthHeight(90, 90);
	m_pPotionHolderImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Potion_Holder (Texture)"));

	m_pPotionHolderImage->GetRectTransform()->Set_Pivot(0.f, 0.f);
	m_pPotionHolderImage->GetRectTransform()->Set_AnchorsMin(0.f, 0.f);
	m_pPotionHolderImage->GetRectTransform()->Set_AnchoredPosition(20.f, 140.f);

	CGameObject* pcBtnObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PCBtn");
	m_pPCBtnImage = pcBtnObj->AddComponent<CImage>();
	pcBtnObj->GetTransform()->SetParent(GetTransform());

	m_pPCBtnImage->GetRectTransform()->Set_WidthHeight(90, 90);
	m_pPCBtnImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Btn_PcEmpty (Texture)"));

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
	stackRect->Set_AnchoredPositonY(26.f);
	stackRect->Set_WidthHeight(27.f, 43.f);

	for (_int i = 0; i < m_sOptions.maxPotionStack; ++i)
	{
		CGameObject* stackObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionStack_" + to_wstring(i));
		m_vPotionStacks.push_back(stackObj->AddComponent<CImage>());
		m_vPotionStacks.back()->GetRectTransform()->SetParent(stackRect);
		m_vPotionStacks.back()->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Potion_Stack (Texture)"));
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

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Full (Texture)");

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
	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Full (Texture)");

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
			hpRect->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
			set.bg.push_back(hpRect);

			CGameObject* hpFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_Dot_" + to_wstring(j));
			CImage* hpFullRect = hpFullObj->AddComponent<CImage>();
			hpFullRect->GetTransform()->SetParent(hpRect->GetRectTransform());
			hpFullRect->SetTexture(fullTex);
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
	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Full (Texture)");

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
				hpRect->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
				set.bg.push_back(hpRect);

				CGameObject* hpFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Rect_Dot_" + to_wstring(k));
				CImage * hpFullRect = hpFullObj->AddComponent<CImage>();
				hpFullRect->GetTransform()->SetParent(hpRect->GetRectTransform());
				hpFullRect->SetTexture(fullTex);
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

void CPlayerHUD::Update_Status(const CPlayer::PlayerStatus& _status)
{
	m_sCachedStatus = _status;

	if (!m_bHpDisplayInitialized)
	{
		m_fDisplayedHp = static_cast<_float>(_status.crtHp);
		m_bHpDisplayInitialized = true;
	}

	Update_BE(_status.maxBetaEnergy, _status.crtBeatEnergy);
	Update_SH(_status.maxShield, _status.crtShield);
	Update_HP(_status.maxHp, static_cast<_uint>(std::round(m_fDisplayedHp)));
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
