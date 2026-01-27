#include "epch.h"
#include "Canvas.h"

CCanvas::CCanvas()
	: m_eRenderMode(RenderMode::ScreenSpace_Overlay)
	, m_lUIObjectList({})
{
	m_strName = L"Canvas";
}

CCanvas::~CCanvas()
{
}

CCanvas* CCanvas::Create()
{
	CCanvas* newCanvas = new CCanvas();
	newCanvas->m_bIsCanvas = true;
	return newCanvas;
}

CComponent* CCanvas::Clone() const
{
	CCanvas* clone = new CCanvas();

	clone->m_eRenderMode = this->m_eRenderMode;

	return clone;
}

HRESULT CCanvas::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	vector2 resolution = vector2(CDisplay::GetInstance().Get_ScreenResolution().x, CDisplay::GetInstance().Get_ScreenResolution().y);

	if (m_eRenderMode == RenderMode::ScreenSpace_Overlay)
	{
		Get_Transform()->Set_Position(10.f, 10.f, 0.f);
		Get_Transform()->Set_EulerAngles(vector3::zero());
		Get_Transform()->Set_LocalScale(resolution.x * 0.01f, resolution.y * 0.01f, 1.f);
	}

	return S_OK;
}

void CCanvas::OnPreRender_Editor()
{
	if (!m_pRectGizmoMesh)
		return;

	vector2 resolution = vector2(CDisplay::GetInstance().Get_ScreenResolution().x, CDisplay::GetInstance().Get_ScreenResolution().y);
	
	if (m_eRenderMode == RenderMode::ScreenSpace_Overlay)
	{
		Get_Transform()->Set_Position(50.f, 50.f, 0.f);
		Get_Transform()->Set_EulerAngles(vector3::zero());
		Get_Transform()->Set_LocalScale(resolution.x * 0.01f, resolution.y * 0.01f, 1.f);
	}
}

void CCanvas::Render_Editor()
{
	CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();

	vector3 cPos = cam->Get_Transform()->Get_Position();
	_float3 camPos = cPos.toFloat3();
	_matrix matWorld = Get_Transform()->Get_WorldMatrix();
	_matrix matView = cam->Get_ViewMatrix();
	_matrix matProj = cam->Get_ProjectionMatrix();

	if (m_pLineMat)
	{
		m_pLineMat->Bind_Matrix(matWorld);
		m_pLineMat->Bind_Camera(camPos, matView, matProj, 0);
	}

	if (m_pRectGizmoMesh)
		m_pRectGizmoMesh->Render();
}

void CCanvas::OnPostRender_Editor()
{
}

void CCanvas::Render()
{
	for (TRAVERSAL_ITER(m_lUIObjectList, it))
	{
		CSceneManager::GetInstance().Get_CrtScene()->Get_Camera()->Add_RenderTarget_UI(*it);
	}
}

void CCanvas::OnDestroy()
{
	for (TRAVERSAL_ITER(m_lUIObjectList, it))
		Safe_Release(*it);

	m_lUIObjectList.clear();
}

void CCanvas::Add_UIObject(CUI* _ui)
{
	if (_ui)
	{
		m_lUIObjectList.push_back(_ui);
		m_lUIObjectList.back()->AddRef();
	}
}

const CCanvas::RenderMode CCanvas::Get_RenderMode() const
{
	return m_eRenderMode;
}
