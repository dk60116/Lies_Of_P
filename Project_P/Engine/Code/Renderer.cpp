#include "epch.h"
#include "Renderer.h"

CRenderer::CRenderer()
	: m_pMaterial(nullptr)
	, m_pOutlineMat(nullptr)
{
}

CRenderer::~CRenderer()
{
}

void CRenderer::Update()
{
	if (m_pMaterial)
	{
		_uint id = m_pGameObject->Get_UniqueID();
		m_pMaterial->Set_IntValue(L"gObjectID", id);
	}
}

void CRenderer::OnDestroy()
{
	Safe_Release(m_pMaterial);
	Safe_Release(m_pOutlineMat);
}

HRESULT CRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	if (!m_pMaterial)
	{
		Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)"));
	}

	if (!m_pOutlineMat)
	{
		m_pOutlineMat = CResources::GetInstance().CloneOnGame<CMaterial>(L"Outline (Material)");
		if (m_pOutlineMat)
			m_pOutlineMat->Set_BaseColor(ColorValue::yellow().f4Color());
	}
	
	if (m_pOutlineMat)
		m_pOutlineMat->AddRef();

	CShader* outShader = CResources::GetInstance().LoadOnGame<CShader>(L"Outline (Shader)");

	if (!outShader)
	{
		CDebug::LogError("Not found outline shader");
		return E_FAIL;
	}

	return S_OK;
}

CMaterial* CRenderer::Get_Material()
{
	return m_pMaterial;
}

void CRenderer::Set_Material(CMaterial* _material)
{
	Safe_Release(m_pMaterial);

	m_pMaterial = _material;

	if (m_pMaterial)
		m_pMaterial->AddRef();
}
