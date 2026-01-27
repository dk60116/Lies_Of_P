#include "epch.h"
#include "UIManager.h"

CUIManager::CUIManager()
	: m_pUIGizmoMaterial(nullptr)
{
}

CUIManager::~CUIManager()
{
	Release();
}

CUIManager& CUIManager::GetInstance()
{
	static CUIManager inst;
	return inst;
}

HRESULT CUIManager::Initialize()
{
#ifndef _CLIENT_BUILD
	if (!m_pUIGizmoMaterial)
	{
		m_pUIGizmoMaterial = CResources::GetInstance().LoadOnGame<CMaterial>(L"DefaultLineMaterial (Material)");
		m_pUIGizmoMaterial->AddRef();
	}
#endif

	return S_OK;
}

void CUIManager::Release()
{
	Safe_Release(m_pUIGizmoMaterial);
}

CMaterial* CUIManager::Get_UIGizmoMaterial() const
{
	return m_pUIGizmoMaterial;
}
