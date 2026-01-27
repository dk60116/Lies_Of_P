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

	}
	
	if (m_pOutlineMat)
		m_pOutlineMat->AddRef();

	CShader* outShader = CResources::GetInstance().LoadOnGame<CShader>(L"Outline (Shader)");

	if (!outShader)
	{
		CDebug::LogError("Not found outline shader");
		return E_FAIL;
	}

	//m_pOutlineMat->Set_Shader(outShader);

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
