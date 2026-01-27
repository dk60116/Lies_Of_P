#include "cpch.h"
#include "PlayerHUD.h"

CPlayerHUD::CPlayerHUD()
	: m_pPlayer(nullptr)
	, m_pCanvas(nullptr)
	, m_sOptions({})
	, m_pHeartContainer(nullptr)
	, m_vHeartBowlList({})
	, m_vHeartImageList({})
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
	CScene* crtScene = m_pGameObject->Get_Scene();

	m_pCanvas = m_pGameObject->AddComponent<CCanvas>();

	CGameObject* heartContainerObj = crtScene->Add_GameObject(L"Hearts");
	m_pHeartContainer = heartContainerObj->AddComponent<CUI>()->Get_RectTransform();
	m_pHeartContainer->SetParent(m_pCanvas->Get_RectTransform());

	CTexture* heartTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HeartBar (Texture)");

	for (_uint i = 0; i < CPlayer::StaticPlayerStatus::HPMAX; ++i)
	{
		const wstring bowlName = wstring(L"Heart bowl ") + L"(" + to_wstring(i) + L")";
		CGameObject* heartBowlObj = crtScene->Add_GameObject(bowlName);
		m_vHeartBowlList.push_back(heartBowlObj->AddComponent<CImage>());
		m_vHeartBowlList.back()->Get_RectTransform()->SetParent(m_pHeartContainer);
		m_vHeartBowlList.back()->SetTexture(heartTex);
		m_vHeartBowlList.back()->SetColor(ColorValue::gray(0.5f));

		const wstring heartName = wstring(L"Heart") + L"(" + to_wstring(i) + L")";
		CGameObject* heartObj = crtScene->Add_GameObject(heartName);
		m_vHeartImageList.push_back(heartObj->AddComponent<CImage>());
		m_vHeartImageList.back()->Get_RectTransform()->SetParent(m_vHeartBowlList.back()->Get_RectTransform());
		m_vHeartImageList.back()->SetTexture(heartTex);
		m_vHeartImageList.back()->SetColor(ColorValue::red());
	}

	CGameManager::GetInstance().Set_PlayerHUD(this);

	return S_OK;
}

void CPlayerHUD::Awake()
{
	m_pPlayer = CGameManager::GetInstance().Get_Player();

	m_pHeartContainer->Set_Pivot(0.f, 1.f);
	m_pHeartContainer->Set_AnchorsMin(0.f, 1.f);
	m_pHeartContainer->Set_AnchoredPosition(20.f, -20.f);
	m_pHeartContainer->Set_WidthHeight(m_sOptions.heartSize * CPlayer::StaticPlayerStatus::HPMAX + m_sOptions.heartSpacing * CPlayer::StaticPlayerStatus::HPMAX, m_sOptions.heartSize);

	for (size_t i = 0; i < m_vHeartBowlList.size(); ++i)
	{
		CRectTransform* bowlRT = m_vHeartBowlList[i]->Get_RectTransform();
		bowlRT->Set_PivotX(0.f);
		bowlRT->Set_AnchorsMin(0.f, 0.5f);
		bowlRT->Set_WidthHeight(m_sOptions.heartSize, m_sOptions.heartSize);
		bowlRT->Set_AnchoredPositonX(m_sOptions.heartSize * i + m_sOptions.heartSpacing * i);

		CRectTransform* heartRT = m_vHeartImageList[i]->Get_RectTransform();
		bowlRT->Set_WidthHeight(m_sOptions.heartSize, m_sOptions.heartSize);
	}
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

void CPlayerHUD::Update_Heart(const _int _current, const _int _max)
{
	if (_current < 0)
		return;

	_float targetHp = _current / 2.f;
	_int other = static_cast<_int>(_current) % 2;

	for (_int i = 0; i < static_cast<_int>(m_vHeartBowlList.size()); ++i)
	{
		m_vHeartBowlList[i]->Get_GameObject()->SetActive(i < static_cast<_int>(_max * 0.5f));
		m_vHeartImageList[i]->Get_GameObject()->SetActive(i < targetHp);
		m_vHeartImageList[i]->SetFillAmount(1.f);
	}

	m_vHeartImageList[static_cast<_int>(targetHp)]->SetFillAmount(other > 0.f ? 0.5f : 0.f);
}
