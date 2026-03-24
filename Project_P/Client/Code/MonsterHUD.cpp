#include "cpch.h"
#include "MonsterHUD.h"
#include "Monster.h"

CMonsterHUD::CMonsterHUD()
	: m_pRect(nullptr)
	, m_pMonster(nullptr)
	, m_pCanvas(nullptr)
{
}

CMonsterHUD::~CMonsterHUD()
{
}

CMonsterHUD* CMonsterHUD::Create()
{
	return new CMonsterHUD();
}

CComponent* CMonsterHUD::Clone() const
{
	CMonsterHUD* clone = new CMonsterHUD();

	return clone;
}

HRESULT CMonsterHUD::Initialize()
{
	m_pRect = m_pGameObject->AddComponent<CRectTransform>();

	GetTransform()->SetParent(CGameManager::GetInstance().Get_PlayerHUD()->GetTransform());

	m_pRect->Set_WidthHeight(150.f, 30.f);
	
	CVerticalLayoutGroup* vlg = m_pGameObject->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, true);

	CreateHPBar();
	CreateSHBar();
	CreateBABar();

	return S_OK;
}

void CMonsterHUD::Awake()
{
}

void CMonsterHUD::Start()
{
}

void CMonsterHUD::Update()
{
	if (m_pMonster)
	{
		const vector2 sp = CGameManager::GetInstance().Get_PlayerCamera()->GetCamera()->WorldToScreenPoint(m_pMonster->GetTransform()->Get_Position());
		const vector2 res = vector2(CDisplay::GetInstance().Get_ScreenResolution().x, CDisplay::GetInstance().Get_ScreenResolution().y);
		m_pRect->Set_AnchoredPosition(sp.x - res.x * 0.5f, res.y * 0.5f - sp.y);
	}
}

void CMonsterHUD::OnDestroy()
{
	Safe_Release(m_pMonster);
}

void CMonsterHUD::BIndMonster(CMonster* _monster)
{
	if (m_pMonster == _monster)
		return;

	if (_monster == nullptr)
		Safe_Release(m_pMonster);

	m_pMonster = _monster;

	if (m_pMonster)
		m_pMonster->AddRef();
}

void CMonsterHUD::CreateHPBar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HPBarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
}

void CMonsterHUD::CreateSHBar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SHBarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
}

void CMonsterHUD::CreateBABar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BABarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
}
