#include "epch.h"
#include "MeshRenderer.h"

namespace
{
	class ScopedRasterizerOverride
	{
	public:
		ScopedRasterizerOverride(ID3D11DeviceContext* _context, ID3D11RasterizerState* _replacement)
			: m_pContext(_context)
			, m_pPrevious(nullptr)
			, m_bApplied(false)
		{
			if (!m_pContext || !_replacement)
				return;

			m_pContext->RSGetState(&m_pPrevious);
			m_pContext->RSSetState(_replacement);
			m_bApplied = true;
		}

		~ScopedRasterizerOverride()
		{
			if (!m_pContext || !m_bApplied)
				return;

			m_pContext->RSSetState(m_pPrevious);
			Safe_Release(m_pPrevious);
		}

	private:
		ID3D11DeviceContext* m_pContext;
		ID3D11RasterizerState* m_pPrevious;
		_bool m_bApplied;
	};
}

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

	clone->m_bCastShadow = this->m_bCastShadow;

	return clone;
}

HRESULT CMeshRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	if (!m_pMeshFilter)
	{
		m_pMeshFilter = m_pGameObject->GetComponent<CMeshFilter>();

		if (!m_pMeshFilter)
			m_pMeshFilter = m_pGameObject->AddComponent<CMeshFilter>();

		if (m_pMeshFilter)
			m_pMeshFilter->AddRef();
	}

	if (!Get_Material())
		Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)"));

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
	CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera()->Add_RenderTarget_Mesh(this);
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

	// MeshBuffer 
	CMeshBuffer* pBuffer = m_pMeshFilter->Get_MeshBuffer();

	if (!pBuffer)
		return;

	// World / View / Projection  

	if (m_pMaterial)
		m_pMaterial->Set_IntValue(L"gObjectID", m_pGameObject->Get_UniqueID());

	vector3 cPos = _cam->GetTransform()->Get_Position();
	_float3 camPos = cPos.toFloat3();
	_matrix matWorld = GetTransform()->GetSnapshotWorldMatrix();
	_matrix matView = _cam->GetViewMatrix();
	_matrix matProj = _cam->GetProjectionMatrix();
	const _bool isMirrored = CRenderer::IsMirroredWorldMatrix(matWorld);
	ScopedRasterizerOverride mirroredRasterizer
	(
		m_pContext,
		isMirrored ? CGraphicDevice::GetInstance().Get_Rasterizer_CullBackMirrored() : nullptr
	);

	// ̴ + ؽó +   ε
	m_pMaterial->Bind_Matrix(matWorld);
	m_pMaterial->Bind_Camera(camPos, matView, matProj, 0);

	Bind_InstanceBuffer(matWorld);
	if (IsInstancingEnabled())
	{
		pBuffer->Render_Instanced(GetInstanceCount());
	}
	else
	{
		pBuffer->Render();
	}
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
	_matrix matWorld = GetTransform()->GetSnapshotWorldMatrix();

	// Light View/Proj (shadow matrices)
	_matrix matView = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(&_shadowMatrix.view));
	_matrix matProj = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(&_shadowMatrix.proj));

	// Shadow depth camPos ǹ Ƿ 
	_float3 dummyPos = { 0.f, 0.f, 0.f };

	_shadowDepthMat->Bind_Matrix(matWorld);
	_shadowDepthMat->Bind_Camera(dummyPos, matView, matProj, 0);

	Bind_InstanceBuffer(matWorld);
	if (IsInstancingEnabled())
	{
		pBuffer->Render_Instanced(GetInstanceCount());
	}
	else
	{
		pBuffer->Render();
	}
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
	if (!m_pMeshFilter)
		return nullptr;

	return m_pMeshFilter->Get_MeshBuffer();
}

const _float CMeshRenderer::GetScaleFactor() const
{
	if (!m_pMeshFilter)
		return 1.f;

	CMeshBuffer* meshBuffer = m_pMeshFilter->Get_MeshBuffer();
	if (!meshBuffer)
		return 1.f;

	return meshBuffer->Get_ScaleFactor();
}
