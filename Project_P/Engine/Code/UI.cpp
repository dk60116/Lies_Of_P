#include "epch.h"
#include "UI.h"

CUI::CUI()
	: m_pRectTransform(nullptr)
	, m_pCanvas(nullptr)
	, m_pRectGizmoMesh(nullptr)
	, m_pLineMat(nullptr)
	, m_pRectMesh(nullptr)
	, m_pMaterial(nullptr)
	, m_bIsCanvas(false)
	, m_vColor(ColorValue::white())
{
}

CUI::~CUI()
{
}

CUI* CUI::Create()
{
	CUI* newCanvas = new CUI();
	return newCanvas;
}

CComponent* CUI::Clone() const
{
	CUI* clone = new CUI();

	return clone;
}

HRESULT CUI::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

#ifndef _CLIENT_BUILD
	if (!m_pRectGizmoMesh)
	{
		m_pRectGizmoMesh = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"LineRect (Mesh Buffer)");
		m_pRectGizmoMesh->AddRef();
	}

	if (!m_pLineMat)
	{
		m_pLineMat = CResources::GetInstance().LoadOnGame<CMaterial>(L"DefaultLineMaterial (Material)");
		m_pLineMat->AddRef();
	}
#endif

	if (!m_pRectTransform)
	{
		m_pRectTransform = m_pGameObject->AddComponent<CRectTransform>();

		if (m_pRectTransform)
		{
			m_pGameObject->Set_Transform(m_pRectTransform);
			m_pRectTransform->Set_UI(this);
			m_pRectTransform->AddRef();
		}
	}

	if (!m_pRectMesh)
		Set_Mesh(CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Rect (Mesh Buffer)"));

	if (!m_pMaterial)
		Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"DefaultUIMaterial (Material)"));

	return S_OK;
}

void CUI::OnDestroy()
{
	Safe_Release(m_pRectGizmoMesh);
	Safe_Release(m_pLineMat);

	Safe_Release(m_pRectTransform);
	Safe_Release(m_pRectMesh);
	Safe_Release(m_pMaterial);
	Safe_Release(m_pCanvas);

	if (m_pCanvas)
		m_pCanvas->Remove_UIObject(this);
}

void CUI::Set_Mesh(CMeshBuffer* _mesh)
{
	if (_mesh == m_pRectMesh)
		return;

	Safe_Release(m_pMaterial);

	m_pRectMesh = _mesh;

	if (m_pRectMesh)
		m_pRectMesh->AddRef();
}

void CUI::Set_Material(CMaterial* _material)
{
	if (_material == m_pMaterial)
		return;

	Safe_Release(m_pMaterial);

	m_pMaterial = _material;

	if (m_pMaterial)
		m_pMaterial->AddRef();
}

void CUI::Bind_Mesh()
{
	m_pRectMesh->Render();
}

void CUI::Bind_Matrix()
{
	m_pMaterial->Bind_Matrix(GetTransform()->Get_WorldMatrix());
}

void CUI::Bind_Camera(const _fmatrix _view, const _cmatrix _projection)
{
	m_pMaterial->Bind_Camera(_float3(), _view, _projection);
}

const _bool CUI::Is_Canvas() const
{
	return m_bIsCanvas;
}

void CUI::UnLinkCanvas()
{
	Safe_Release(m_pCanvas);
	m_pCanvas = nullptr;
}

CCanvas* CUI::Get_Canvas() const
{
	return m_pCanvas;
}

ColorValue CUI::GetColor() const
{
	return m_vColor;
}

void CUI::Set_Canvas(CCanvas* _canvas)
{
	if (_canvas == m_pCanvas)
		return;

	Safe_Release(m_pCanvas);

	m_pCanvas = _canvas;

	if (m_pCanvas)
		m_pCanvas->AddRef();
}

CRectTransform* CUI::GetRectTransform() const
{
	return m_pRectTransform;
}

void CUI::SetColor(ColorValue _color)
{
	m_vColor = _color;
	m_pMaterial->Set_BaseColor(m_vColor.f4Color());
}

void CUI::SetAlpha(const _float _value)
{
	m_vColor.a = static_cast<BYTE>(_value * 255);
	SetColor(m_vColor);
}
