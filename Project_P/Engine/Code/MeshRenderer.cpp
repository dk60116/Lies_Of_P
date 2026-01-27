#include "epch.h"
#include "MeshRenderer.h"

CMeshRenderer::CMeshRenderer()
	: CRenderer{}
	, m_pMeshFilter(nullptr)
{
	m_strName = L"Mesh Renderer";
}

CMeshRenderer::~CMeshRenderer()
{
}

CMeshRenderer* CMeshRenderer::Create()
{
	return new CMeshRenderer();
}

CComponent* CMeshRenderer::Clone() const
{
	CMeshRenderer* clone = new CMeshRenderer();

	return clone;
}

HRESULT CMeshRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	if (!m_pMeshFilter)
	{
		m_pMeshFilter = m_pGameObject->AddComponent<CMeshFilter>();

		if (m_pMeshFilter)
			m_pMeshFilter->AddRef();
	}

	return S_OK;
}

void CMeshRenderer::OnPreCull()
{
}

void CMeshRenderer::OnPreRender()
{
}

void CMeshRenderer::Render_Editor()
{
	m_pContext->OMSetDepthStencilState(CSceneManager::GetInstance().Get_CrtScene()->Get_MeshStencillState(), 0);
	Render_WithCamera(CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera());
}

void CMeshRenderer::Render()
{
	CSceneManager::GetInstance().Get_CrtScene()->Get_Camera()->Add_RenderTarget_Mesh(this);
}

void CMeshRenderer::OnPostRender()
{
}

void CMeshRenderer::OnDestroy()
{
	__super::OnDestroy();

	Safe_Release(m_pMeshFilter);
}

void CMeshRenderer::Render_WithCamera(CCamera* _cam)
{
	if (!_cam)
	{
		CDebug::LogError(L"MeshRenderer: No Camera assigned." + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMeshFilter)
	{
		CDebug::LogError(L"MeshRenderer: No MeshFilter assigned:" + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMaterial)
	{
		CDebug::LogError(L"MeshRenderer: No material assigned: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	// MeshBuffer 가져오기
	CMeshBuffer* pBuffer = m_pMeshFilter->Get_MeshBuffer();

	if (!pBuffer)
		return;

	// World / View / Projection 행렬 계산

	vector3 cPos = _cam->Get_Transform()->Get_Position();
	_float3 camPos = cPos.toFloat3();
	_matrix matWorld = Get_Transform()->Get_WorldMatrix();
	_matrix matView = _cam->Get_ViewMatrix();
	_matrix matProj = _cam->Get_ProjectionMatrix();

	// 셰이더 + 텍스처 + 상수 버퍼 바인딩
	m_pMaterial->Bind_Matrix(matWorld);
	m_pMaterial->Bind_Camera(camPos, matView, matProj, 0);

	//if (m_pMaterial->IsUseLight())
	//{
	//	list<CLight*> lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightList();
	//	const _uint lightCount = static_cast<_uint>(lights.size());

	//	vector<_matrix> vLightInfos = {};

	//	_uint index = 0;

	//	for (TRAVERSAL_ITER(lights, it))
	//	{
	//		if (!(*it))
	//			continue;

	//		_float4x4 lightInfo = (*it)->To_LightInfo();
	//		
	//		if (index == 0)
	//			lightInfo._44 = static_cast<_float>(lights.size());
	//		else
	//			lightInfo._44 = 0.f;

	//		vLightInfos.push_back(XMLoadFloat4x4(&lightInfo));

	//		++index;
	//	}

	//	m_pMaterial->Bind_Light(vLightInfos.data(), static_cast<_uint>(lights.size()));
	//}

	//실제 메쉬 렌더링 (버퍼 바인딩 및 Draw)
	pBuffer->Render();
}

void CMeshRenderer::Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatrix)
{
	if (!_shadowDepthMat)
	{
		CDebug::LogError(L"MeshRenderer::Render_ShadowDepth - shadowDepthMat is null: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMeshFilter)
	{
		CDebug::LogError(L"MeshRenderer::Render_ShadowDepth - No MeshFilter: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	CMeshBuffer* pBuffer = m_pMeshFilter->Get_MeshBuffer();
	if (!pBuffer)
		return;

	// World
	_matrix matWorld = Get_Transform()->Get_WorldMatrix();

	// Light View/Proj (shadow matrices)
	_matrix matView = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(&_shadowMatrix.view));
	_matrix matProj = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(&_shadowMatrix.proj));

	// Shadow depth는 camPos 의미 없으므로 더미
	_float3 dummyPos = { 0.f, 0.f, 0.f };

	_shadowDepthMat->Bind_Matrix(matWorld);
	_shadowDepthMat->Bind_Camera(dummyPos, matView, matProj, 0);

	pBuffer->Render();
}

void CMeshRenderer::Render_Outline(CCamera* _cam)
{
}

CMeshFilter* CMeshRenderer::Get_MeshFilter()
{
	return m_pMeshFilter;
}

CMeshBuffer* CMeshRenderer::Get_MeshBuffer()
{
	return m_pMeshFilter->Get_MeshBuffer();
}
