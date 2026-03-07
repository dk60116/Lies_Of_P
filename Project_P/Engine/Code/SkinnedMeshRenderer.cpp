#include "epch.h"
#include "SkinnedMeshRenderer.h"
#include "EditorCamera.h"

CSkinnedMeshRenderer::CSkinnedMeshRenderer()
	: CRenderer{}
	, m_pMeshBuffer(nullptr)
	, m_vBones({})
	, m_vRootBone({})
	, m_pBoneMatrixBuffer(nullptr)
	, m_bSkinningCacheValid(false)
	, m_iCachedBoneCount(0)
	, m_iCachedPoseHash(0)
	, m_vCachedSkinMatrices({})
	, m_vCachedBoneMatrices({})
{
	m_strName = L"Skinned Mesh Renderer";
	InvalidateSkinningCache();
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
		if (b)
			b->AddRef();
		clone->m_vBones.push_back(b);
	}

	clone->m_vRootBone = this->m_vRootBone;
	for (auto* r : clone->m_vRootBone)
		r->AddRef();

	clone->m_pBoneMatrixBuffer = nullptr;
	clone->InvalidateSkinningCache();
	return clone;
}

HRESULT CSkinnedMeshRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

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
	InvalidateSkinningCache();
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

uint64_t CSkinnedMeshRenderer::ComputeSkinningPoseHash(_uint _boneCount) const
{
	uint64_t hash = 1469598103934665603ull;
	const uint64_t prime = 1099511628211ull;

	auto hashBytes = [&hash, prime](const void* data, size_t size)
	{
		const _ubyte* bytes = reinterpret_cast<const _ubyte*>(data);
		for (size_t i = 0; i < size; ++i)
		{
			hash ^= bytes[i];
			hash *= prime;
		}
	};

	hashBytes(&_boneCount, sizeof(_boneCount));

	_float4x4 meshWorld = {};
	if (m_pGameObject && m_pGameObject->Get_Transform())
		XMStoreFloat4x4(&meshWorld, m_pGameObject->Get_Transform()->Get_WorldMatrix());
	hashBytes(&meshWorld, sizeof(meshWorld));

	for (_uint i = 0; i < _boneCount; ++i)
	{
		_float4x4 boneWorld = {};
		if (i < m_vBones.size() && m_vBones[i])
			XMStoreFloat4x4(&boneWorld, m_vBones[i]->Get_WorldMatrix());
		hashBytes(&boneWorld, sizeof(boneWorld));
	}

	return hash;
}

void CSkinnedMeshRenderer::InvalidateSkinningCache() const
{
	m_bSkinningCacheValid = false;
	m_iCachedBoneCount = 0;
	m_iCachedPoseHash = 0;
	m_vCachedSkinMatrices.clear();

	if (m_vCachedBoneMatrices.size() != MAX_BONE)
		m_vCachedBoneMatrices.resize(MAX_BONE);

	const _matrix identity = XMMatrixIdentity();
	for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
		XMStoreFloat4x4(&cachedBoneMatrix, identity);
}

_bool CSkinnedMeshRenderer::TryUpdateSkinningCache(_uint* _outBoneCount) const
{
	if (_outBoneCount)
		*_outBoneCount = 0;

	if (!m_pMeshBuffer)
		return false;

	const _uint meshBoneCount = m_pMeshBuffer->Get_BoneCount();
	const _uint offsetBoneCount = static_cast<_uint>(m_pMeshBuffer->m_vBoneOffsetMatrices.size());
	const _uint boneCount = min<_uint>(min<_uint>(static_cast<_uint>(m_vBones.size()), meshBoneCount), min<_uint>(offsetBoneCount, MAX_BONE));

	if (_outBoneCount)
		*_outBoneCount = boneCount;

	if (m_vCachedBoneMatrices.size() != MAX_BONE)
		m_vCachedBoneMatrices.resize(MAX_BONE);

	if (boneCount == 0)
	{
		const _matrix identity = XMMatrixIdentity();
		for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
			XMStoreFloat4x4(&cachedBoneMatrix, identity);

		m_vCachedSkinMatrices.clear();
		m_iCachedBoneCount = 0;
		m_iCachedPoseHash = 1ull;
		m_bSkinningCacheValid = true;
		return true;
	}

	const uint64_t poseHash = ComputeSkinningPoseHash(boneCount);
	if (m_bSkinningCacheValid
		&& m_iCachedBoneCount == boneCount
		&& m_iCachedPoseHash == poseHash
		&& m_vCachedSkinMatrices.size() == boneCount)
	{
		return true;
	}

	if (m_vCachedSkinMatrices.size() != boneCount)
		m_vCachedSkinMatrices.resize(boneCount);

	const _matrix identity = XMMatrixIdentity();
	for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
		XMStoreFloat4x4(&cachedBoneMatrix, identity);

	_matrix meshWorld = XMMatrixIdentity();
	_matrix meshWorldInv = XMMatrixIdentity();
	if (m_pGameObject && m_pGameObject->Get_Transform())
	{
		meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
		meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
	}

	for (_uint i = 0; i < boneCount; ++i)
	{
		_matrix skinMatrix = XMMatrixIdentity();
		if (m_vBones[i])
		{
			const _matrix invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);
			const _matrix boneWorld = m_vBones[i]->Get_WorldMatrix();
			const _matrix boneMeshLocal = boneWorld * meshWorldInv;
			skinMatrix = invBindPose * boneMeshLocal;
		}

		XMStoreFloat4x4(&m_vCachedSkinMatrices[i], skinMatrix);
		XMStoreFloat4x4(&m_vCachedBoneMatrices[i], XMMatrixTranspose(skinMatrix));
	}

	m_iCachedBoneCount = boneCount;
	m_iCachedPoseHash = poseHash;
	m_bSkinningCacheValid = true;
	return true;
}

_bool CSkinnedMeshRenderer::UploadBoneMatricesFromCache() const
{
	if (!m_pBoneMatrixBuffer || m_vCachedBoneMatrices.size() != MAX_BONE)
		return false;

	D3D11_MAPPED_SUBRESOURCE mappedRes = {};
	const HRESULT hr = m_pContext->Map(m_pBoneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	if (FAILED(hr))
		return false;

	memcpy(mappedRes.pData, m_vCachedBoneMatrices.data(), sizeof(_float4x4) * MAX_BONE);
	m_pContext->Unmap(m_pBoneMatrixBuffer, 0);
	return true;
}

_bool CSkinnedMeshRenderer::TryGetAnimatedWorldBounds(_float3& _outMin, _float3& _outMax) const
{
	if (!m_pMeshBuffer || !m_pMeshBuffer->m_pVertexSysMem)
		return false;

	const CMeshBuffer::MESHBUFFERDESC& info = m_pMeshBuffer->Get_Info();
	if (info.vertexSize != sizeof(VertexSkinnedBuffer) || info.vertextCount == 0)
		return false;

	_uint boneCount = 0;
	if (!TryUpdateSkinningCache(&boneCount) || boneCount == 0)
		return false;

	auto* vertices = static_cast<const VertexSkinnedBuffer*>(m_pMeshBuffer->m_pVertexSysMem);

	_matrix meshWorld = XMMatrixIdentity();
	if (m_pGameObject && m_pGameObject->Get_Transform())
		meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
	_bool hasPoint = false;

	for (_uint v = 0; v < info.vertextCount; ++v)
	{
		const VertexSkinnedBuffer& src = vertices[v];
		const _vector p = XMVectorSet(src.position.x, src.position.y, src.position.z, 1.f);
		_vector skinned = XMVectorZero();
		_float totalW = 0.f;

		for (_uint k = 0; k < 4; ++k)
		{
			const _uint idx = src.boneIndices[k];
			const _float w = src.boneWeights[k];
			if (w <= 0.f || idx >= boneCount)
				continue;

			const _matrix skinMatrix = XMLoadFloat4x4(&m_vCachedSkinMatrices[idx]);
			skinned = XMVectorAdd(skinned, XMVectorScale(XMVector3Transform(p, skinMatrix), w));
			totalW += w;
		}

		if (totalW <= 0.f)
			continue;

		skinned = XMVectorScale(skinned, 1.f / totalW);
		const _vector worldSkinned = XMVector3Transform(skinned, meshWorld);

		hasPoint = true;
		minV = XMVectorMin(minV, worldSkinned);
		maxV = XMVectorMax(maxV, worldSkinned);
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
		if (m_vBones.size() <= idx)
			m_vBones.resize(idx + 1, nullptr);
		m_vBones[idx] = boneTf;
		boneTf->AddRef();
	}

	InvalidateSkinningCache();

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

	m_pMaterial->Set_IntValue(L"gObjectID", m_pGameObject->Get_UniqueID());

	vector3 cPos = _cam->Get_Transform()->Get_Position();
	const _float3 camPos = cPos.toFloat3();

	const _matrix matWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
	const _matrix matView = _cam->Get_ViewMatrix();
	const _matrix matProj = _cam->Get_ProjectionMatrix();

	_uint boneCount = 0;
	if (!TryUpdateSkinningCache(&boneCount))
	{
		CDebug::LogError(L"Skinned MeshRenderer - Failed to build skinning cache: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pBoneMatrixBuffer)
	{
		CDebug::LogError(L"Skinned MeshRenderer - BoneMatrixBuffer is null: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!UploadBoneMatricesFromCache())
	{
		CDebug::LogError(L"Skinned MeshRenderer - Failed Map BoneMatrixBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	Bind_InstanceBuffer(matWorld);
	m_pMaterial->Bind_Matrix(matWorld);
	m_pMaterial->Bind_Camera(camPos, matView, matProj, boneCount);
	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

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

	const _matrix matWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
	const _matrix matView = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.view));
	const _matrix matProj = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.proj));

	_uint boneCount = 0;
	if (!TryUpdateSkinningCache(&boneCount))
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - Failed to build skinning cache: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!UploadBoneMatricesFromCache())
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - Failed Map BoneMatrixBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

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
		InvalidateSkinningCache();
		return;
	}

	m_pMeshBuffer->AddRef();
	InvalidateSkinningCache();
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

	InvalidateSkinningCache();
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
		InvalidateSkinningCache();
	}
}

const _float CSkinnedMeshRenderer::GetScaleFactor() const
{
	return m_pMeshBuffer->Get_ScaleFactor();
}
