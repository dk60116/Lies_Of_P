#include "epch.h"
#include "Canvas.h"
#include "GameObject.h"
#include "Transform.h"

namespace
{
	void CollectCanvasUIInHierarchyOrder(CTransform* parentTransform, CCanvas* canvas, CCamera* camera)
	{
		if (!parentTransform || !canvas || !camera)
			return;

		for (CTransform* childTransform : parentTransform->Get_ChldList())
		{
			if (!childTransform)
				continue;

			CGameObject* childObject = childTransform->Get_GameObject();
			if (!childObject)
				continue;

			if (CUI* ui = childObject->GetComponent<CUI>())
			{
				if (ui != canvas && ui->Get_Canvas() == canvas)
					camera->Add_RenderTarget_UI(ui);
			}

			CollectCanvasUIInHierarchyOrder(childTransform, canvas, camera);
		}
	}
}

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
		GetTransform()->Set_Position(10.f, 10.f, 0.f);
		GetTransform()->Set_EulerAngles(vector3::zero());
		GetTransform()->Set_LocalScale(resolution.x * 0.01f, resolution.y * 0.01f, 1.f);
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
		GetTransform()->Set_Position(50.f, 50.f, 0.f);
		GetTransform()->Set_EulerAngles(vector3::zero());
		GetTransform()->Set_LocalScale(resolution.x * 0.01f, resolution.y * 0.01f, 1.f);
	}
}

void CCanvas::Render_Editor()
{
	CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();

	if (!cam)
		return;

	vector3 cPos = cam->GetTransform()->Get_Position();
	_float3 camPos = cPos.toFloat3();
	_matrix matWorld = GetTransform()->Get_WorldMatrix();
	_matrix matView = cam->GetViewMatrix();
	_matrix matProj = cam->GetProjectionMatrix();

	if (m_pLineMat)
	{
		m_pLineMat->Bind_Matrix(matWorld);
		m_pLineMat->Bind_Camera(camPos, matView, matProj, 0);
	}

	if (m_pRectGizmoMesh)
		m_pRectGizmoMesh->Render();

	CollectCanvasUIInHierarchyOrder(GetTransform(), this, cam);
}

void CCanvas::OnPostRender_Editor()
{
}

void CCanvas::Render()
{
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!scene)
		return;

	CCamera* camera = scene->Get_Camera();
	if (!camera)
		return;

	CollectCanvasUIInHierarchyOrder(GetTransform(), this, camera);
}

void CCanvas::OnDestroy()
{
	__super::OnDestroy();

	for (TRAVERSAL_ITER(m_lUIObjectList, it))
	{
		(*it)->UnLinkCanvas();
		Safe_Release(*it);
	}

	m_lUIObjectList.clear();
}

void CCanvas::Add_UIObject(CUI* _ui)
{
	if (_ui)
	{
		if (find(m_lUIObjectList.begin(), m_lUIObjectList.end(), _ui) != m_lUIObjectList.end())
			return;

		m_lUIObjectList.push_back(_ui);
		m_lUIObjectList.back()->AddRef();
	}
}

void CCanvas::Remove_UIObject(CUI* _ui)
{
	if (_ui)
	{
		for (auto it = m_lUIObjectList.begin(); it != m_lUIObjectList.end();)
		{
			if (*it == _ui)
			{
				Safe_Release(*it);
				it = m_lUIObjectList.erase(it);
			}
			else
				++it;
		}
	}
}

const CCanvas::RenderMode CCanvas::Get_RenderMode() const
{
	return m_eRenderMode;
}
