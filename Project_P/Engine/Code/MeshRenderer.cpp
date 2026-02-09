#include "epch.h"
#include "MeshRenderer.h"

CMeshRenderer::CMeshRenderer()
	: CRenderer{}
	, m_pMeshFilter(nullptr)
	, m_pInstanceBuffer(nullptr)
	, m_iInstanceCount(0)
	, m_bInstancing(false)
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
	clone->m_bInstancing = this->m_bInstancing;
	clone->m_iInstanceCount = this->m_iInstanceCount;
	clone->m_vInstanceTransforms = this->m_vInstanceTransforms;

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
	Safe_Release(m_pInstanceBuffer);
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

	UpdateInstanceBuffer(matWorld);

	if (m_bInstancing && m_iInstanceCount > 0)
		pBuffer->Render_Instanced(m_iInstanceCount);
	else
		pBuffer->Render();
	CMeshBuffer* pBuffer = m_pMeshFilter->Get_MeshBuffer();

void CMeshRenderer::CreateMeshInstancing(_uint _count)
{
	if (_count == 0)
	{
		m_bInstancing = false;
		m_iInstanceCount = 0;
		m_vInstanceTransforms.clear();
		return;
	}

	if (_count > 256)
		_count = 256;

	m_bInstancing = true;
	m_iInstanceCount = _count;
	m_vInstanceTransforms.assign(_count, InstanceTransform{ vector3::zero(), vector3::zero(), vector3::one() });

	CreateInstanceBuffer();
}

void CMeshRenderer::SetInstancingPosition(_uint _index, const vector3& _position)
{
	if (_index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].position = _position;
}

void CMeshRenderer::SetInstancingRotation(_uint _index, const vector3& _rotation)
{
	if (_index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].rotation = _rotation;
}

void CMeshRenderer::SetInstancingSize(_uint _index, const vector3& _size)
{
	if (_index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].scale = _size;
}

void CMeshRenderer::UpdateInstanceBuffer(const _matrix& _baseWorld)
{
	CreateInstanceBuffer();

	if (!m_pInstanceBuffer)
		return;

	InstanceCB buffer = {};
	buffer.useInstancing = m_bInstancing && m_iInstanceCount > 0;
	buffer.instanceCount = m_bInstancing ? m_iInstanceCount : 0;

	if (!m_bInstancing || m_iInstanceCount == 0)
	{
		buffer.instanceWorlds[0] = XMMatrixTranspose(_baseWorld);
	}
	else
	{
		const _uint count = min(m_iInstanceCount, static_cast<_uint>(m_vInstanceTransforms.size()));
		for (_uint i = 0; i < count; ++i)
		{
			_matrix world = BuildInstanceWorld(m_vInstanceTransforms[i]);
			world = world * _baseWorld;
			buffer.instanceWorlds[i] = XMMatrixTranspose(world);
		}
	}

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	context->UpdateSubresource(m_pInstanceBuffer, 0, nullptr, &buffer, 0, 0);
	context->VSSetConstantBuffers(11, 1, &m_pInstanceBuffer);
}

void CMeshRenderer::CreateInstanceBuffer()
{
	if (m_pInstanceBuffer)
		return;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(InstanceCB);
	device->CreateBuffer(&desc, nullptr, &m_pInstanceBuffer);
}

_matrix CMeshRenderer::BuildInstanceWorld(const InstanceTransform& _transform) const
{
	_vector scale = XMVectorSet(_transform.scale.x, _transform.scale.y, _transform.scale.z, 0.f);
	_matrix scaleMat = XMMatrixScalingFromVector(scale);
	quaternion rot = _transform.rotation.to_quaternion();
	_matrix rotMat = XMMatrixRotationQuaternion(rot.toXMVector());
	_matrix transMat = XMMatrixTranslation(_transform.position.x, _transform.position.y, _transform.position.z);
	return scaleMat * rotMat * transMat;
}

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

const _float CMeshRenderer::GetScaleFactor() const
{
	return m_pMeshFilter->Get_MeshBuffer()->Get_ScaleFactor();
}
