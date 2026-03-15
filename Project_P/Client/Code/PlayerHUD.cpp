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
	CGameObject* beObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Gauge_BE");
	CRectTransform* rect = beObj->AddComponent<CRectTransform>();
	beObj->GetTransform()->SetParent(GetTransform());

	CreateGauge_BE(rect);
}

void CPlayerHUD::CreateGauge_BE(CRectTransform* _parent)
{
	CGameObject* gaugeTextObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BE_Text");
	gaugeTextObj->AddComponent<CText>();
	gaugeTextObj->GetTransform()->SetParent(_parent);
}
