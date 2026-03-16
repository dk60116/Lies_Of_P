#include "cpch.h"
#include "PlayerHUD.h"

CPlayerHUD::CPlayerHUD()
	: m_pPlayer(nullptr)
	, m_pCanvas(nullptr)
	, m_sOptions({})
	, m_pAimImage(nullptr)
	, m_pPotionHolderImage(nullptr)
	, m_pPCBtnImage(nullptr)
	, m_pTextPotionCount(nullptr)
	, m_vGaugeTexts({})
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
}

void CPlayerHUD::Start()
{
}

void CPlayerHUD::Update()
{
}

void CPlayerHUD::OnDestroy()
{
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
}

void CPlayerHUD::CreateGauge()
{
	CGameObject* gaugeObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauges");
	CRectTransform* rect = gaugeObj->AddComponent<CRectTransform>();
	gaugeObj->GetTransform()->SetParent(GetTransform());
	rect->Set_Pivot(0.f, 0.f);
	rect->Set_AnchorsMin(0.f, 0.f);
	rect->Set_AnchoredPosition(110.f, 55.f);
	rect->Set_AnchoredSize(m_sOptions.gaugeWidthMax, 75.f);

	CGameObject* beObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_BE");
	CRectTransform* rect_be = beObj->AddComponent<CRectTransform>();
	beObj->GetTransform()->SetParent(rect);
	CRectTransform* beGR = CreateGauge_Default(L"BE", rect_be);
	CreateGauge_BE(beGR);

	CGameObject* hpObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_HP");
	CRectTransform* rect_hp = hpObj->AddComponent<CRectTransform>();
	rect_hp->GetTransform()->SetParent(rect);
	CRectTransform* hpGR = CreateGauge_Default(L"HP", rect_hp);
	CreateGauge_HP(hpGR);

	CGameObject* shObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_SH");
	CRectTransform* rect_sh = shObj->AddComponent<CRectTransform>();
	rect_sh->GetTransform()->SetParent(rect);
	CRectTransform* shGR = CreateGauge_Default(L"SH", rect_sh);
	CreateGauge_SH(shGR);

	CVerticalLayoutGroup* vlg = rect->Get_GameObject()->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, true);
	vlg->SetSpacing(8.f);
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

	for (_int i = 0; i < 10; ++i)
	{
		CGameObject* gaugeImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BE_GaugeImage_Empty_" + to_wstring(i));
		CImage* empty = gaugeImageObj->AddComponent<CImage>();
		gaugeImageObj->GetTransform()->SetParent(_parent);
		empty->SetTexture(emptyTex);
		empty->GetRectTransform()->Set_AnchoredSize(14.f, 14.f);

		CGameObject* fullImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BE_GaugeImage_Full_" + to_wstring(i));
		CImage* full = fullImageObj->AddComponent<CImage>();
		fullImageObj->GetTransform()->SetParent(gaugeImageObj->GetTransform());
		full->SetTexture(fullTex);
		full->SetColor(ColorValue(204, 233, 244));
		full->GetRectTransform()->Set_AnchoredSize(empty->GetRectTransform()->Get_AnchoredSize());
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	const CLayoutGroup::Padding p = { 0.f, 0.f, 3.f, 3.f };
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetPadding(p);
	hlg->SetSpacing(4.f);
}

void CPlayerHUD::CreateGauge_HP(CRectTransform* _parent)
{
	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Full (Texture)");

	for (_int i = 0; i < 65; ++i)
	{
		CGameObject* hpRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_" + to_wstring(i));
		CRectTransform* rect = hpRectObj->AddComponent<CRectTransform>();
		rect->SetParent(_parent);
		rect->Set_AnchoredSizeX(7.f);

		for (_int j = 0; j < 3; ++j)
		{
			CGameObject* hpObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HP_Rect_Dot_" + to_wstring(j));
			CImage* hpRect = hpObj->AddComponent<CImage>();
			hpRect->GetTransform()->SetParent(rect);
			hpRect->GetRectTransform()->Set_AnchoredSize(5.f, 5.f);
		}

		CVerticalLayoutGroup* vlg = hpRectObj->AddComponent<CVerticalLayoutGroup>();
		vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		vlg->SetSpacing(2.f);
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetSpacing(2.f);
	hlg->SetControlChildSize(false, true);
}

void CPlayerHUD::CreateGauge_SH(CRectTransform* _parent)
{
	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Beta_Cube_Full (Texture)");

	for (_int i = 0; i < 5; ++i)
	{
		CGameObject* hpRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_Rect_" + to_wstring(i));
		CRectTransform* rect = hpRectObj->AddComponent<CRectTransform>();
		rect->SetParent(_parent);
		rect->Set_AnchoredSizeX(68.f);

		for (_int j = 0; j < 5; ++j)
		{
			CGameObject* gaugeImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_GaugeImage_Empty_" + to_wstring(i));
			CImage* empty = gaugeImageObj->AddComponent<CImage>();
			gaugeImageObj->GetTransform()->SetParent(rect);
			empty->SetTexture(emptyTex);
			empty->GetRectTransform()->Set_AnchoredSize(12.f, 12.f);

			CGameObject* fullImageObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SH_GaugeImage_Full_" + to_wstring(i));
			CImage* full = fullImageObj->AddComponent<CImage>();
			fullImageObj->GetTransform()->SetParent(gaugeImageObj->GetTransform());
			full->SetTexture(fullTex);
			full->SetColor(ColorValue(173, 209, 196));
			full->GetRectTransform()->Set_AnchoredSize(empty->GetRectTransform()->Get_AnchoredSize());

			CHorizontalLayoutGroup* hlg = hpRectObj->AddComponent<CHorizontalLayoutGroup>();
			hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
			hlg->SetSpacing(2.f);
		}
	}

	CHorizontalLayoutGroup* hlg = _parent->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	const CLayoutGroup::Padding p = { 0.f, 0.f, 3.f, 3.f };
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetPadding(p);
	hlg->SetSpacing(10.f);
}
