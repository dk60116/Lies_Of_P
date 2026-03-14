#include "cpch.h"
#include "PlayerHUD.h"

CPlayerHUD::CPlayerHUD()
	: m_pPlayer(nullptr)
	, m_pCanvas(nullptr)
	, m_sOptions({})
	, m_pAimImage(nullptr)
	, m_pPotionHolderImage(nullptr)
	, m_pPCBtnImage(nullptr)
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
	aimImageObj->Get_Transform()->SetParent(Get_Transform());

	m_pAimImage->Get_RectTransform()->Set_WidthHeight(3, 3);
	m_pAimImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Aim (Texture)"));
}

void CPlayerHUD::CreatePotions()
{
	CGameObject* potionHolderObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PotionHolder");
	m_pPotionHolderImage = potionHolderObj->AddComponent<CImage>();
	potionHolderObj->Get_Transform()->SetParent(Get_Transform());

	m_pPotionHolderImage->Get_RectTransform()->Set_WidthHeight(90, 90);
	m_pPotionHolderImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Potion_Holder (Texture)"));

	m_pPotionHolderImage->Get_RectTransform()->Set_Pivot(0.f, 0.f);
	m_pPotionHolderImage->Get_RectTransform()->Set_AnchorsMin(0.f, 0.f);
	m_pPotionHolderImage->Get_RectTransform()->Set_AnchoredPosition(20.f, 120.f);

	CGameObject* pcBtnObj = m_pGameObject->Get_Scene()->Add_GameObject(L"PCBtn");
	m_pPCBtnImage = pcBtnObj->AddComponent<CImage>();
	pcBtnObj->Get_Transform()->SetParent(Get_Transform());

	m_pPCBtnImage->Get_RectTransform()->Set_WidthHeight(90, 90);
	m_pPCBtnImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_Btn_PcEmpty (Texture)"));

	m_pPCBtnImage->Get_RectTransform()->Set_Pivot(0.f, 0.f);
	m_pPCBtnImage->Get_RectTransform()->Set_AnchorsMin(0.f, 0.f);
	m_pPCBtnImage->Get_RectTransform()->Set_AnchoredPosition(20.f, 60.f);
}
