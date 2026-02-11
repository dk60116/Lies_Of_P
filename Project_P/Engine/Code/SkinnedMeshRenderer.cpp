#include "epch.h"
#include "SkinnedMeshRenderer.h"
#include "EditorCamera.h"

CSkinnedMeshRenderer::CSkinnedMeshRenderer()
	: CRenderer{}
	, m_pMeshBuffer(nullptr)
	, m_vBones({})
	, m_vRootBone({})
	, m_pBoneMatrixBuffer(nullptr)
{
	m_strName = L"Skinned Mesh Renderer";
}

CSkinnedMeshRenderer::~CSkinnedMeshRenderer()
{
}

CSkinnedMeshRenderer* CSkinnedMeshRenderer::Create()
{
	return new CSkinnedMeshRenderer();
}

CComponent* CSkinnedMeshRenderer::Clone() const
{
	auto* clone = new CSkinnedMeshRenderer();

	clone->m_bCastShadow = this->m_bCastShadow;

	clone->m_pMeshBuffer = this->m_pMeshBuffer;
	if (clone->m_pMeshBuffer)
		clone->m_pMeshBuffer->AddRef();

	clone->m_vBones.clear();
	clone->m_vBones.reserve(this->m_vBones.size());
	for (auto* b : this->m_vBones)
	{
		if (b) b->AddRef();
		clone->m_vBones.push_back(b);
	}

	clone->m_vRootBone = this->m_vRootBone;
	
	for (auto r : clone->m_vRootBone)
		r->AddRef();

	clone->m_pBoneMatrixBuffer = this->m_pBoneMatrixBuffer;
	if (clone->m_pBoneMatrixBuffer)
		clone->m_pBoneMatrixBuffer->AddRef();

	return clone;
}

HRESULT CSkinnedMeshRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	auto mat = m_pMaterial;

	//      
	D3D11_BUFFER_DESC desc = {};
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(_matrix) * MAX_BONE;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pBoneMatrixBuffer)))
		return E_FAIL;

	return S_OK;
}

void CSkinnedMeshRenderer::OnPreCull()
{
}

void CSkinnedMeshRenderer::OnPreRender()
{
}

void CSkinnedMeshRenderer::Render_Editor()
{
	m_pContext->OMSetDepthStencilState(CSceneManager::GetInstance().Get_CrtScene()->Get_MeshStencillState(), 0);
	CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera()->Add_RenderTarget_Mesh(this);
}

void CSkinnedMeshRenderer::Render()
{
	CSceneManager::GetInstance().Get_CrtScene()->Get_Camera()->Add_RenderTarget_Mesh(this);
}

void CSkinnedMeshRenderer::OnPostRender()
{
}

void CSkinnedMeshRenderer::OnDestroy()
{
	__super::OnDestroy();

	Safe_Release(m_pBoneMatrixBuffer);
	Safe_Release(m_pMeshBuffer);

	for (TRAVERSAL_ITER(m_vBones, it))
		Safe_Release(*it);

	for (TRAVERSAL_ITER(m_vRootBone, it))
		Safe_Release(*it);
	
	m_vBones.clear();
	m_vRootBone.clear();

	m_vBones.clear();
}

const _uint CSkinnedMeshRenderer::Get_BoneCount() const
{
	return static_cast<_uint>(m_vBones.size());
}

const wstring CSkinnedMeshRenderer::Get_BoneName(const _uint _index) const
{
	return m_vBones[_index]->Get_GameObject()->Get_ObjectName();
}

CTransform* CSkinnedMeshRenderer::Get_BoneTransform(const _uint _index) const
{
	return m_vBones[_index];
}

const _float4x4& CSkinnedMeshRenderer::Get_BoneOffsetMatrix(const _uint _index) const
{
	return m_pMeshBuffer->Get_BoneOffsetMatrix(_index);
}


_bool CSkinnedMeshRenderer::TryGetAnimatedWorldBounds(_float3& _outMin, _float3& _outMax) const
{
	if (!m_pMeshBuffer || !m_pMeshBuffer->m_pVertexSysMem)
		return false;

	const CMeshBuffer::MESHBUFFERDESC& info = m_pMeshBuffer->Get_Info();
	if (info.vertexSize != sizeof(VertexSkinnedBuffer) || info.vertextCount == 0)
		return false;

	auto* vertices = static_cast<const VertexSkinnedBuffer*>(m_pMeshBuffer->m_pVertexSysMem);

	const _uint boneCount = min<_uint>(static_cast<_uint>(m_vBones.size()), m_pMeshBuffer->Get_BoneCount());
	if (boneCount == 0)
		return false;

	_matrix meshWorldInv = XMMatrixIdentity();
	if (m_pGameObject && m_pGameObject->Get_Transform())
	{
		_matrix meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
		meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
	}

	vector<_matrix> skinMats(boneCount, XMMatrixIdentity());
	for (_uint i = 0; i < boneCount; ++i)
	{
		if (!m_vBones[i])
			continue;

		_matrix invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);
		_matrix boneWorld = m_vBones[i]->Get_WorldMatrix();
		_matrix boneMeshLocal = boneWorld * meshWorldInv;
		skinMats[i] = invBindPose * boneMeshLocal;
	}

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
	_bool hasPoint = false;

	for (_uint v = 0; v < info.vertextCount; ++v)
	{
		const VertexSkinnedBuffer& src = vertices[v];
		_vector p = XMVectorSet(src.position.x, src.position.y, src.position.z, 1.f);
		_vector skinned = XMVectorZero();
		_float totalW = 0.f;

		for (_uint k = 0; k < 4; ++k)
		{
			const _uint idx = src.boneIndices[k];
			const _float w = src.boneWeights[k];
			if (w <= 0.f || idx >= boneCount)
				continue;

			skinned += XMVector3Transform(p, skinMats[idx]) * w;
			totalW += w;
		}

		if (totalW <= 0.f)
			continue;

		hasPoint = true;
		minV = XMVectorMin(minV, skinned);
		maxV = XMVectorMax(maxV, skinned);
	}

	if (!hasPoint)
		return false;

	XMStoreFloat3(&_outMin, minV);
	XMStoreFloat3(&_outMax, maxV);
	return true;
}
void CSkinnedMeshRenderer::CreateBoneHierachy(const vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>& nodes, _int nodeIdx, CTransform* parentTf)
{
	const auto& n = nodes[nodeIdx];

	CGameObject* boneGO = m_pGameObject->Get_Scene()->Add_GameObject(n.name);
	CTransform* boneTf = boneGO->Get_Transform();

	if (parentTf)
		boneTf->SetParent(parentTf);

	_matrix m = XMLoadFloat4x4(&n.transformation);
	_vector S, R, T;
	XMMatrixDecompose(&S, &R, &T, m);
	boneTf->Set_LocalScale(S);
	boneTf->Set_LocalQuaternion(R);
	boneTf->Set_LocalPosition(T);

	auto it = find(m_pMeshBuffer->m_vBoneNames.begin(),
		m_pMeshBuffer->m_vBoneNames.end(),
		n.name);
	if (it != m_pMeshBuffer->m_vBoneNames.end())
	{
		size_t idx = static_cast<size_t>(distance(m_pMeshBuffer->m_vBoneNames.begin(), it));
		if (m_vBones.size() <= idx) m_vBones.resize(idx + 1, nullptr);
		m_vBones[idx] = boneTf;
		boneTf->AddRef();
	}

	for (auto childId : n.childsId)
		CreateBoneHierachy(nodes, childId, boneTf);
}

void CSkinnedMeshRenderer::Render_WithCamera(CCamera* _cam)
{
	if (!_cam)
	{
		CDebug::LogError("Skinned MeshRenderer: No Camera assigned.");
		return;
	}

	if (!m_pMaterial)
	{
		CDebug::LogError(L"Skinned MeshRenderer - No material assigned: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMeshBuffer)
	{
		CDebug::LogError(L"Skinned MeshRenderer - No MeshBuffer assigned :" + m_pGameObject->Get_ObjectNameID());
		return;
	}

	vector3 cPos = _cam->Get_Transform()->Get_Position();
	_float3 camPos = cPos.toFloat3();

	_matrix matWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
	_matrix matView = _cam->Get_ViewMatrix();
	_matrix matProj = _cam->Get_ProjectionMatrix();

	const _uint boneCount = min<_uint>(static_cast<_uint>(m_vBones.size()), MAX_BONE);

	_matrix boneMatrices[MAX_BONE];
	for (_int i = 0; i < MAX_BONE; ++i)
		boneMatrices[i] = XMMatrixIdentity();

	_matrix meshWorldInv = XMMatrixIdentity();
	{
		if (m_pGameObject && m_pGameObject->Get_Transform())
		{
			_matrix meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
			meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
		}
	}

	for (_uint i = 0; i < boneCount; ++i)
	{
		if (!m_vBones[i])
			continue;

		//   
		_matrix boneWorld = m_vBones[i]->Get_WorldMatrix();

		//  ε (Offset)
		// (m_vBoneOffsetMatrices ε m_vBones   )
		_matrix invBindPose = XMMatrixIdentity();
		invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);

		// bone mesh local ȯ
		// (boneWorld * meshWorldInv) : boneWorld  meshLocal
		_matrix boneMeshLocal = boneWorld * meshWorldInv;

		//   
		// (invBindPose * currentBone)  
		boneMatrices[i] = XMMatrixTranspose(invBindPose * boneMeshLocal);
	}

	if (!m_pBoneMatrixBuffer)
	{
		CDebug::LogError(L"Skinned MeshRenderer - BoneMatrixBuffer is null: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mappedRes = {};
	HRESULT hrMap = m_pContext->Map(m_pBoneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	if (SUCCEEDED(hrMap))
	{
		memcpy(mappedRes.pData, boneMatrices, sizeof(_matrix) * MAX_BONE);
		m_pContext->Unmap(m_pBoneMatrixBuffer, 0);
	}
	else
	{
		CDebug::LogError(L"Skinned MeshRenderer - Failed Map BoneMatrixBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	Bind_InstanceBuffer(matWorld);

	// 6) Material bind (boneCount Ŭ )
	m_pMaterial->Bind_Matrix(matWorld);
	m_pMaterial->Bind_Camera(camPos, matView, matProj, boneCount);

	// 7) Bones CB bind (b3)
	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

	// 8) Draw
	if (IsInstancingEnabled())
		m_pMeshBuffer->Render_Instanced(GetInstanceCount());
	else
		m_pMeshBuffer->Render();
}

void CSkinnedMeshRenderer::Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatrix)
{
	if (!_shadowDepthMat)
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - shadowDepthMat is null: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMeshBuffer)
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - No MeshBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pBoneMatrixBuffer)
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - BoneMatrixBuffer is null: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	_matrix matWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();

	_matrix matView = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.view));
	_matrix matProj = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.proj));

	const _uint boneCount = min<_uint>(static_cast<_uint>(m_vBones.size()), MAX_BONE);

	_matrix boneMatrices[MAX_BONE];
	for (int i = 0; i < MAX_BONE; ++i)
		boneMatrices[i] = XMMatrixIdentity();

	_matrix meshWorldInv = XMMatrixIdentity();
	{
		if (m_pGameObject && m_pGameObject->Get_Transform())
		{
			_matrix meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
			meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
		}
	}

	for (_uint i = 0; i < boneCount; ++i)
	{
		if (!m_vBones[i])
			continue;

		_matrix boneWorld = m_vBones[i]->Get_WorldMatrix();
		_matrix invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);

		_matrix boneMeshLocal = boneWorld * meshWorldInv;

		boneMatrices[i] = XMMatrixTranspose(invBindPose * boneMeshLocal);
	}

	D3D11_MAPPED_SUBRESOURCE mappedRes = {};
	HRESULT hr = m_pContext->Map(m_pBoneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	if (FAILED(hr))
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - Failed Map BoneMatrixBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	memcpy(mappedRes.pData, boneMatrices, sizeof(_matrix) * MAX_BONE);
	m_pContext->Unmap(m_pBoneMatrixBuffer, 0);

	_float3 dummyPos = { 0.f, 0.f, 0.f };
	Bind_InstanceBuffer(matWorld);
	_shadowDepthMat->Bind_Matrix(matWorld);
	_shadowDepthMat->Bind_Camera(dummyPos, matView, matProj, boneCount);

	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

	if (IsInstancingEnabled())
		m_pMeshBuffer->Render_Instanced(GetInstanceCount());
	else
		m_pMeshBuffer->Render();
}

void CSkinnedMeshRenderer::Render_Outline(CCamera* _cam)
{
}

CMeshBuffer* CSkinnedMeshRenderer::Get_MeshBuffer()
{
	return m_pMeshBuffer;
}

CSkinnedMeshBuffer* CSkinnedMeshRenderer::Get_SkinnedMeshBuffer()
{
	return m_pMeshBuffer;
}

void CSkinnedMeshRenderer::Set_MeshBuffer(CSkinnedMeshBuffer* _mesh)
{
	if (_mesh == m_pMeshBuffer)
		return;

	Safe_Release(m_pMeshBuffer);

	m_pMeshBuffer = _mesh;

	if (!m_pMeshBuffer)
	{
		CDebug::LogError("Skinned MeshRenderer - Set_Mesh Failed - Skinned MeshRenderer: No MeshBuffer");
		return;
	}

	m_pMeshBuffer->AddRef();
}

void CSkinnedMeshRenderer::Set_Bones(const vector<CTransform*>& _bones, vector<CTransform*> _rootBone)
{
	for (auto* t : m_vBones)
		Safe_Release(t);

	m_vBones.clear();

	for (TRAVERSAL_ITER(m_vRootBone, it))
		Safe_Release(*it);

	m_vRootBone = {};

	m_vBones.reserve(_bones.size());
	for (auto* t : _bones)
	{
		if (t) 
			t->AddRef();
		m_vBones.push_back(t);
	}
	
	m_vRootBone = _rootBone;

	for (TRAVERSAL_ITER(m_vRootBone, it))
		(*it)->AddRef();
}

vector<CTransform*>& CSkinnedMeshRenderer::GetRootBons()
{
	return m_vRootBone;
}

const wstring CSkinnedMeshRenderer::Get_RootBoneName(const _int _index) const
{
	return m_vRootBone[_index]->Get_GameObject()->Get_ObjectName();
}

void CSkinnedMeshRenderer::AddRootBone(CTransform* _tf)
{
	if (_tf)
	{
		m_vRootBone.push_back(_tf);
		m_vRootBone.back()->AddRef();
	}
}

const _float CSkinnedMeshRenderer::GetScaleFactor() const
{
	return m_pMeshBuffer->Get_ScaleFactor();
}
