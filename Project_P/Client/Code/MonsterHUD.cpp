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

	m_pRect->Set_WidthHeight(90.f, 18.f);
	
	CVerticalLayoutGroup* vlg = m_pGameObject->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, true);
	vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::UpperCenter);

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
		const vector2 sp = CGameManager::GetInstance().Get_PlayerCamera()->GetCamera()->WorldToScreenPoint(m_pMonster->GetTransform()->Get_Position() + (vector3::up() * (m_pMonster->GetHeight() + 0.5f)));
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

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Full (Texture)");

	const _float rectSize = 3.f;
	const _int rectCount = 30;

	for (_int i = 0; i < rectCount; ++i)
	{
		CGameObject* hpRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_HP_Rect_" + to_wstring(i));
		CRectTransform* hpRect = hpRectObj->AddComponent<CRectTransform>();
		hpRect->SetParent(rect);
		hpRect->Set_Width(rectSize);

		HPGaugeSet set = {};
		set.rect = hpRect;

		for (_int j = 0; j < 2; ++j)
		{
			CGameObject* hpEmptyObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_HP_Empty_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* hpEmptyImg = hpEmptyObj->AddComponent<CImage>();
			hpEmptyImg->GetTransform()->SetParent(hpRect);
			hpEmptyImg->SetTexture(emptyTex);
			hpEmptyImg->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
			set.bg.push_back(hpEmptyImg);

			CGameObject* hpFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_HP_Full_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* hpFullImg = hpFullObj->AddComponent<CImage>();
			hpFullImg->GetTransform()->SetParent(hpEmptyImg->GetRectTransform());
			hpFullImg->SetTexture(fullTex);
			hpFullImg->Set_FillMethod(CImage::FillMethod::Horizontal);
			hpFullImg->GetRectTransform()->Set_WidthHeight(hpEmptyImg->GetRectTransform()->Get_WidthHeight());
			set.fill.push_back(hpFullImg);
		}

		m_vHPBox.push_back(set);

		CVerticalLayoutGroup* vlg = hpRectObj->AddComponent<CVerticalLayoutGroup>();
		vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		vlg->SetSpacing(0.f);
	}

	CHorizontalLayoutGroup* hlg = rectObj->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetSpacing(0.f);

	CHorizontalLayoutGroup* vlg = rect->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
}

void CMonsterHUD::CreateSHBar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SHBarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Full (Texture)");

	const _float rectSize = 3.f;
	const _int rectCount = 30;

	for (_int i = 0; i < rectCount; ++i)
	{
		CGameObject* shRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_SH_Rect_" + to_wstring(i));
		CRectTransform* shRect = shRectObj->AddComponent<CRectTransform>();
		shRect->SetParent(rect);
		shRect->Set_Width(rectSize);

		SEGaugeSet set = {};
		set.rect = shRect;

		for (_int j = 0; j < 2; ++j)
		{
			CGameObject* shEmptyObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_SH_Empty_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* shEmptyImg = shEmptyObj->AddComponent<CImage>();
			shEmptyImg->GetTransform()->SetParent(shRect);
			shEmptyImg->SetTexture(emptyTex);
			shEmptyImg->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
			set.bg.push_back(shEmptyImg);

			CGameObject* shFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_SH_Full_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* shFullImg = shFullObj->AddComponent<CImage>();
			shFullImg->GetTransform()->SetParent(shEmptyImg->GetRectTransform());
			shFullImg->SetTexture(fullTex);
			shFullImg->SetColor(ColorValue(173, 209, 196));
			shFullImg->Set_FillMethod(CImage::FillMethod::Horizontal);
			shFullImg->GetRectTransform()->Set_WidthHeight(shEmptyImg->GetRectTransform()->Get_WidthHeight());
			set.fill.push_back(shFullImg);
		}

		m_vSHBox.push_back(set);

		CVerticalLayoutGroup* vlg = shRectObj->AddComponent<CVerticalLayoutGroup>();
		vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		vlg->SetSpacing(0.f);
	}

	CHorizontalLayoutGroup* hlg = rectObj->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetSpacing(0.f);

	CHorizontalLayoutGroup* vlg = rect->Get_GameObject()->AddComponent<CHorizontalLayoutGroup>();
	vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
}

void CMonsterHUD::CreateBABar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BABarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
}

void CMonsterHUD::Update_HP(const _int _maxValue, const _int _current)
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

void CMonsterHUD::Update_SH(const _int _maxValue, const _int _current)
{
	const _float rectCapacity = 100.f;

	for (size_t i = 0; i < m_vSHBox.size(); ++i)
	{
		SEGaugeSet& set = m_vSHBox[i];
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
