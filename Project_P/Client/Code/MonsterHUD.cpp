#include "cpch.h"
#include "MonsterHUD.h"
#include "Monster.h"

namespace
{
	vector2 ToAnchoredPosition(const vector2& _screenPoint, const vector2& _resolution)
	{
		return vector2
		(
			_screenPoint.x - _resolution.x * 0.5f,
			_resolution.y * 0.5f - _screenPoint.y
		);
	}

	_bool IsValidScreenPoint(const vector2& _screenPoint)
	{
		return _screenPoint.x > -FLT_MAX && _screenPoint.y > -FLT_MAX;
	}
}

CMonsterHUD::CMonsterHUD()
	: m_pRect(nullptr)
	, m_pMonster(nullptr)
	, m_pCanvas(nullptr)
	, m_pHPBarRect(nullptr)
	, m_pSHBarRect(nullptr)
	, m_pBABarRect(nullptr)
	, m_sOption({})
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

	m_pRect->Set_WidthHeight(90.f, m_sOption.rectSize * 3.f);

	CVerticalLayoutGroup* vlg = m_pGameObject->AddComponent<CVerticalLayoutGroup>();
	vlg->SetControlChildSize(true, false);
	vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::UpperCenter);

	CreateHPBar();
	CreateSHBar();
	CreateBABar();

	UpdateRectWidthFromMaxHP();

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
	if (!m_pMonster || !m_pRect)
		return;

	CPlayerCamera* playerCamera = CGameManager::GetInstance().Get_PlayerCamera();
	CCamera* camera = playerCamera ? playerCamera->GetCamera() : nullptr;
	if (!camera)
	{
		SetHUDVisible(false);
		return;
	}

	const vector3 worldPoint = m_pMonster->GetTransform()->Get_Position() + (vector3::up() * (m_pMonster->GetHeight() + 1.f));
	const vector2 resolution = vector2(CDisplay::GetInstance().Get_ScreenResolution().x, CDisplay::GetInstance().Get_ScreenResolution().y);
	const vector2 screenPoint = camera->WorldToScreenPoint(worldPoint);

	if (!IsValidScreenPoint(screenPoint))
	{
		SetHUDVisible(false);
		return;
	}

	const CMonster::MonsterStatus& status = m_pMonster->GetStatus();
	UpdateRectWidthFromMaxHP();
	Update_HP(status.maxHp, status.crtHp);
	Update_SH(status.maxShield, status.crtShield);
	Update_BA(status.maxBalance, status.crtBalance);

	SetHUDVisible(true);
	m_pRect->Set_AnchoredPosition(ToAnchoredPosition(screenPoint, resolution));
}

void CMonsterHUD::OnDestroy()
{
	Safe_Release(m_pMonster);
	m_pMonster = nullptr;
	m_pRect = nullptr;
	m_pCanvas = nullptr;
	m_pHPBarRect = nullptr;
	m_pSHBarRect = nullptr;
	m_pBABarRect = nullptr;

	vector<HPGaugeSet>().swap(m_vHPBox);
	vector<SEGaugeSet>().swap(m_vSHBox);
	vector<SEGaugeSet>().swap(m_vBABox);
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

	UpdateRectWidthFromMaxHP();
}

void CMonsterHUD::CreateHPBar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HPBarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
	m_pHPBarRect = rect;

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_HP_Cube_Full (Texture)");

	const _float rectSize = m_sOption.rectSize;
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

	rect->Set_Height(m_sOption.rectSize * 2.f);
}

void CMonsterHUD::CreateSHBar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"SHBarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
	m_pSHBarRect = rect;

	rect->Set_Width(m_sOption.rectSize);

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_SH_Cube_Full (Texture)");

	const _float rectSize = m_sOption.rectSize;
	const _int rectCount = 30;

	for (_int i = 0; i < rectCount; ++i)
	{
		CGameObject* shRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_SH_Rect_" + to_wstring(i));
		CRectTransform* shRect = shRectObj->AddComponent<CRectTransform>();
		shRect->SetParent(rect);
		shRect->Set_Width(rectSize);

		SEGaugeSet set = {};
		set.rect = shRect;

		for (_int j = 0; j < 1; ++j)
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

	rect->Set_Height(m_sOption.rectSize);
}

void CMonsterHUD::CreateBABar()
{
	CGameObject* rectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"BABarRect");
	CRectTransform* rect = rectObj->AddComponent<CRectTransform>();
	rect->SetParent(m_pRect);
	m_pBABarRect = rect;

	CTexture* emptyTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_BA_Cube_Empty (Texture)");
	CTexture* fullTex = CResources::GetInstance().LoadOnScene<CTexture>(L"HUD_BA_Cube_Full (Texture)");

	const _float rectSize = m_sOption.rectSize;
	const _int rectCount = 30;

	for (_int i = 0; i < rectCount; ++i)
	{
		CGameObject* baRectObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_BA_Rect_" + to_wstring(i));
		CRectTransform* baRect = baRectObj->AddComponent<CRectTransform>();
		baRect->SetParent(rect);
		baRect->Set_Width(rectSize);

		SEGaugeSet set = {};
		set.rect = baRect;

		for (_int j = 0; j < 1; ++j)
		{
			CGameObject* baEmptyObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_BA_Empty_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* baEmptyImg = baEmptyObj->AddComponent<CImage>();
			baEmptyImg->GetTransform()->SetParent(baRect);
			baEmptyImg->SetTexture(emptyTex);
			baEmptyImg->GetRectTransform()->Set_WidthHeight(rectSize, rectSize);
			set.bg.push_back(baEmptyImg);

			CGameObject* baFullObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Monster_BA_Full_" + to_wstring(i) + L"_" + to_wstring(j));
			CImage* baFullImg = baFullObj->AddComponent<CImage>();
			baFullImg->GetTransform()->SetParent(baEmptyImg->GetRectTransform());
			baFullImg->SetTexture(fullTex);
			baFullImg->SetColor(ColorValue(239, 255, 155));
			baFullImg->Set_FillMethod(CImage::FillMethod::Horizontal);
			baFullImg->GetRectTransform()->Set_WidthHeight(baEmptyImg->GetRectTransform()->Get_WidthHeight());
			set.fill.push_back(baFullImg);
		}

		m_vBABox.push_back(set);

		CVerticalLayoutGroup* vlg = baRectObj->AddComponent<CVerticalLayoutGroup>();
		vlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleCenter);
		vlg->SetSpacing(0.f);
	}

	CHorizontalLayoutGroup* hlg = rectObj->AddComponent<CHorizontalLayoutGroup>();
	hlg->SetControlChildSize(false, true);
	hlg->SetChildAlignment(CLayoutGroup::ChildAlignment::MiddleLeft);
	hlg->SetSpacing(0.f);

	rect->Set_Height(m_sOption.rectSize);
}

void CMonsterHUD::SetHUDVisible(const _bool _visible)
{
	if (m_pHPBarRect && m_pHPBarRect->Get_GameObject())
		m_pHPBarRect->Get_GameObject()->SetActive(_visible);

	if (m_pSHBarRect && m_pSHBarRect->Get_GameObject())
		m_pSHBarRect->Get_GameObject()->SetActive(_visible);

	if (m_pBABarRect && m_pBABarRect->Get_GameObject())
		m_pBABarRect->Get_GameObject()->SetActive(_visible);
}

void CMonsterHUD::UpdateRectWidthFromMaxHP()
{
	if (!m_pRect || !m_pMonster)
		return;

	const _float hpRectCapacity = 100.f;
	const _float hpRectWidth = m_sOption.rectSize;
	const _float maxHp = static_cast<_float>(max(0, m_pMonster->GetStatus().maxHp));
	const _float activeRectCount = ceilf(maxHp / hpRectCapacity);

	m_pRect->Set_Width(max(hpRectWidth, activeRectCount * hpRectWidth));
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
		const _bool rectFilled = rectCurrent > 0.f;

		set.rect->Get_GameObject()->SetActive(rectActive);
		if (!rectActive || set.fill.empty())
			continue;

		for (CImage* fillImage : set.fill)
		{
			if (!fillImage)
				continue;

			fillImage->SetFillAmount(1.f);
			fillImage->Get_GameObject()->SetActive(rectFilled);
		}
	}
}

void CMonsterHUD::Update_BA(const _int _maxValue, const _int _current)
{
	const _float rectCapacity = 1.f;

	for (size_t i = 0; i < m_vBABox.size(); ++i)
	{
		SEGaugeSet& set = m_vBABox[i];
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
