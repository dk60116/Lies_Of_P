#include "epch.h"
#include "SkinnedMeshRenderer.h"
#include "EditorCamera.h"
#include <cstring>
#include <unordered_map>
#include <cstdlib>

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

	struct TransformLocalPose
	{
		vector3 position = vector3::zero();
		vector3 scale = vector3::one();
		quaternion rotation = quaternion::identity();
	};

	struct SkinningRuntimeCache
	{
		_bool animatedLocalBoundsValid = false;
		_bool animatedWorldBoundsValid = false;
		_bool meshWorldMatrixValid = false;
		uint64_t skinningPoseVersion = 0;
		uint64_t uploadedPoseVersion = 0;
		uint64_t cachedLocalBoundsPoseVersion = 0;
		uint64_t cachedWorldBoundsPoseVersion = 0;
		TransformLocalPose cachedMeshLocalPose = {};
		_float4x4 cachedMeshWorldMatrix = {};
		_float3 cachedAnimatedLocalBoundsMin = {};
		_float3 cachedAnimatedLocalBoundsMax = {};
		_float3 cachedAnimatedWorldBoundsMin = {};
		_float3 cachedAnimatedWorldBoundsMax = {};
		vector<TransformLocalPose> cachedBoneLocalPoses = {};
	};

	unordered_map<const CSkinnedMeshRenderer*, SkinningRuntimeCache>*& GetSkinningRuntimeCachesPtr()
	{
		static unordered_map<const CSkinnedMeshRenderer*, SkinningRuntimeCache>* caches = nullptr;
		return caches;
	}

	bool& IsSkinningRuntimeCacheShutdown()
	{
		static _bool shutdown = false;
		return shutdown;
	}

	void CleanupSkinningRuntimeCachesAtExit()
	{
		IsSkinningRuntimeCacheShutdown() = true;

		auto*& caches = GetSkinningRuntimeCachesPtr();
		delete caches;
		caches = nullptr;
	}

	unordered_map<const CSkinnedMeshRenderer*, SkinningRuntimeCache>* GetSkinningRuntimeCaches()
	{
		if (IsSkinningRuntimeCacheShutdown())
			return nullptr;

		auto*& caches = GetSkinningRuntimeCachesPtr();
		if (!caches)
		{
			caches = new unordered_map<const CSkinnedMeshRenderer*, SkinningRuntimeCache>();
			atexit(&CleanupSkinningRuntimeCachesAtExit);
		}

		return caches;
	}

	SkinningRuntimeCache& GetSkinningRuntimeCache(const CSkinnedMeshRenderer* _renderer)
	{
		static SkinningRuntimeCache dummyCache = {};

		auto* caches = GetSkinningRuntimeCaches();
		if (!caches)
			return dummyCache;

		return (*caches)[_renderer];
	}

	void RemoveSkinningRuntimeCache(const CSkinnedMeshRenderer* _renderer)
	{
		auto* caches = GetSkinningRuntimeCachesPtr();
		if (!caches)
			return;

		caches->erase(_renderer);
	}

	void AdvanceCacheVersion(uint64_t& _version)
	{
		++_version;
		if (_version == 0ull)
			_version = 1ull;
	}

	_bool IsSameFloat4x4(const _float4x4& _lhs, const _float4x4& _rhs)
	{
		return 0 == memcmp(&_lhs, &_rhs, sizeof(_float4x4));
	}

	TransformLocalPose CaptureLocalPose(CTransform* _transform)
	{
		TransformLocalPose pose = {};
		if (!_transform)
			return pose;

		pose.position = _transform->Get_LocalPosition();
		pose.scale = _transform->Get_LocalScale();
		pose.rotation = _transform->Get_LocalQuaternion();
		return pose;
	}

	_bool IsSameLocalPose(const TransformLocalPose& _lhs, const TransformLocalPose& _rhs)
	{
		return _lhs.position == _rhs.position
			&& _lhs.scale == _rhs.scale
			&& _lhs.rotation == _rhs.rotation;
	}

	void BuildWorldBoundsFromLocalAABB(const _float3& _localMin, const _float3& _localMax, const _matrix& _world, _float3& _outMin, _float3& _outMax)
	{
		const _vector corners[8] =
		{
			XMVectorSet(_localMin.x, _localMin.y, _localMin.z, 1.f),
			XMVectorSet(_localMax.x, _localMin.y, _localMin.z, 1.f),
			XMVectorSet(_localMax.x, _localMax.y, _localMin.z, 1.f),
			XMVectorSet(_localMin.x, _localMax.y, _localMin.z, 1.f),
			XMVectorSet(_localMin.x, _localMin.y, _localMax.z, 1.f),
			XMVectorSet(_localMax.x, _localMin.y, _localMax.z, 1.f),
			XMVectorSet(_localMax.x, _localMax.y, _localMax.z, 1.f),
			XMVectorSet(_localMin.x, _localMax.y, _localMax.z, 1.f)
		};

		_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
		_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
		for (_uint i = 0; i < 8; ++i)
		{
			const _vector worldCorner = XMVector3TransformCoord(corners[i], _world);
			minV = XMVectorMin(minV, worldCorner);
			maxV = XMVectorMax(maxV, worldCorner);
		}

		XMStoreFloat3(&_outMin, minV);
		XMStoreFloat3(&_outMax, maxV);
	}
}
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
}

CSkinnedMeshRenderer::~CSkinnedMeshRenderer()
{
	RemoveSkinningRuntimeCache(this);
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
	if (!IsLODVisible())
		return;

	m_pContext->OMSetDepthStencilState(CSceneManager::GetInstance().Get_CrtScene()->Get_MeshStencillState(), 0);
	CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera()->Add_RenderTarget_Mesh(this);
}

void CSkinnedMeshRenderer::Render()
{
	if (!IsLODVisible())
		return;

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
	RemoveSkinningRuntimeCache(this);
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
	if (m_pGameObject && m_pGameObject->GetTransform())
		XMStoreFloat4x4(&meshWorld, m_pGameObject->GetTransform()->GetSnapshotWorldMatrix());
	hashBytes(&meshWorld, sizeof(meshWorld));

	for (_uint i = 0; i < _boneCount; ++i)
	{
		_float4x4 boneWorld = {};
		if (i < m_vBones.size() && m_vBones[i])
			XMStoreFloat4x4(&boneWorld, m_vBones[i]->GetSnapshotWorldMatrix());
		hashBytes(&boneWorld, sizeof(boneWorld));
	}

	return hash;
}

void CSkinnedMeshRenderer::InvalidateSkinningCache() const
{
	auto& cache = GetSkinningRuntimeCache(this);

	m_bSkinningCacheValid = false;
	m_iCachedBoneCount = 0;
	m_iCachedPoseHash = 0;
	m_vCachedSkinMatrices.clear();

	if (m_vCachedBoneMatrices.size() != MAX_BONE)
		m_vCachedBoneMatrices.resize(MAX_BONE);

	const _matrix identity = XMMatrixIdentity();
	for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
		XMStoreFloat4x4(&cachedBoneMatrix, identity);

	cache.animatedLocalBoundsValid = false;
	cache.animatedWorldBoundsValid = false;
	cache.meshWorldMatrixValid = false;
	cache.skinningPoseVersion = 0;
	cache.uploadedPoseVersion = 0;
	cache.cachedLocalBoundsPoseVersion = 0;
	cache.cachedWorldBoundsPoseVersion = 0;
	cache.cachedMeshLocalPose = {};
	cache.cachedMeshWorldMatrix = {};
	cache.cachedBoneLocalPoses.clear();
}
_bool CSkinnedMeshRenderer::TryUpdateSkinningCache(_uint* _outBoneCount) const
{
	if (_outBoneCount)
		*_outBoneCount = 0;

	if (!m_pMeshBuffer)
		return false;

	auto& cache = GetSkinningRuntimeCache(this);

	const _uint meshBoneCount = m_pMeshBuffer->Get_BoneCount();
	const _uint offsetBoneCount = static_cast<_uint>(m_pMeshBuffer->m_vBoneOffsetMatrices.size());
	const _uint boneCount = min<_uint>(min<_uint>(static_cast<_uint>(m_vBones.size()), meshBoneCount), min<_uint>(offsetBoneCount, MAX_BONE));

	if (_outBoneCount)
		*_outBoneCount = boneCount;

	if (m_vCachedBoneMatrices.size() != MAX_BONE)
		m_vCachedBoneMatrices.resize(MAX_BONE);

	CTransform* meshTransform = (m_pGameObject) ? m_pGameObject->GetTransform() : nullptr;
	const TransformLocalPose meshLocalPose = CaptureLocalPose(meshTransform);

	_bool poseUnchanged = m_bSkinningCacheValid
		&& m_iCachedBoneCount == boneCount
		&& m_vCachedSkinMatrices.size() == boneCount
		&& cache.cachedBoneLocalPoses.size() == boneCount
		&& IsSameLocalPose(cache.cachedMeshLocalPose, meshLocalPose);

	if (poseUnchanged)
	{
		for (_uint i = 0; i < boneCount; ++i)
		{
			if (!IsSameLocalPose(cache.cachedBoneLocalPoses[i], CaptureLocalPose(m_vBones[i])))
			{
				poseUnchanged = false;
				break;
			}
		}
	}

	if (poseUnchanged)
		return true;

	if (boneCount == 0)
	{
		const _matrix identity = XMMatrixIdentity();
		for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
			XMStoreFloat4x4(&cachedBoneMatrix, identity);

		cache.cachedBoneLocalPoses.clear();
		cache.cachedMeshLocalPose = meshLocalPose;
		m_vCachedSkinMatrices.clear();
		m_iCachedBoneCount = 0;
		m_iCachedPoseHash = 1ull;
		m_bSkinningCacheValid = true;
		cache.animatedLocalBoundsValid = false;
		cache.animatedWorldBoundsValid = false;
		cache.meshWorldMatrixValid = false;
		cache.uploadedPoseVersion = 0;
		AdvanceCacheVersion(cache.skinningPoseVersion);
		return true;
	}

	if (cache.cachedBoneLocalPoses.size() != boneCount)
		cache.cachedBoneLocalPoses.resize(boneCount);
	if (m_vCachedSkinMatrices.size() != boneCount)
		m_vCachedSkinMatrices.resize(boneCount);

	const _matrix identity = XMMatrixIdentity();
	for (auto& cachedBoneMatrix : m_vCachedBoneMatrices)
		XMStoreFloat4x4(&cachedBoneMatrix, identity);

	_matrix meshWorld = XMMatrixIdentity();
	_matrix meshWorldInv = XMMatrixIdentity();
	if (meshTransform)
	{
		meshWorld = meshTransform->GetSnapshotWorldMatrix();
		meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
	}

	for (_uint i = 0; i < boneCount; ++i)
	{
		cache.cachedBoneLocalPoses[i] = CaptureLocalPose(m_vBones[i]);

		_matrix skinMatrix = XMMatrixIdentity();
		if (m_vBones[i])
		{
			const _matrix invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);
			const _matrix boneWorld = m_vBones[i]->GetSnapshotWorldMatrix();
			const _matrix boneMeshLocal = boneWorld * meshWorldInv;
			skinMatrix = invBindPose * boneMeshLocal;
		}

		XMStoreFloat4x4(&m_vCachedSkinMatrices[i], skinMatrix);
		XMStoreFloat4x4(&m_vCachedBoneMatrices[i], XMMatrixTranspose(skinMatrix));
	}

	m_iCachedBoneCount = boneCount;
	m_iCachedPoseHash = 0;
	cache.cachedMeshLocalPose = meshLocalPose;
	m_bSkinningCacheValid = true;
	cache.animatedLocalBoundsValid = false;
	cache.animatedWorldBoundsValid = false;
	cache.meshWorldMatrixValid = false;
	cache.uploadedPoseVersion = 0;
	AdvanceCacheVersion(cache.skinningPoseVersion);
	return true;
}
_bool CSkinnedMeshRenderer::UploadBoneMatricesFromCache() const
{
	if (!m_pBoneMatrixBuffer || m_vCachedBoneMatrices.size() != MAX_BONE)
		return false;

	auto& cache = GetSkinningRuntimeCache(this);
	if (cache.skinningPoseVersion != 0ull && cache.uploadedPoseVersion == cache.skinningPoseVersion)
		return true;

	const _uint uploadCount = m_iCachedBoneCount > 0 ? m_iCachedBoneCount : MAX_BONE;

	D3D11_MAPPED_SUBRESOURCE mappedRes = {};
	const HRESULT hr = m_pContext->Map(m_pBoneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	if (FAILED(hr))
		return false;

	memcpy(mappedRes.pData, m_vCachedBoneMatrices.data(), sizeof(_float4x4) * uploadCount);
	m_pContext->Unmap(m_pBoneMatrixBuffer, 0);
	cache.uploadedPoseVersion = cache.skinningPoseVersion;
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

	auto& cache = GetSkinningRuntimeCache(this);

	if (!cache.animatedLocalBoundsValid || cache.cachedLocalBoundsPoseVersion != cache.skinningPoseVersion)
	{
		_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
		_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
		_bool hasPoint = false;

		const auto& boneSpheres = m_pMeshBuffer->m_vBoneBoundSpheres;
		const _bool useBoneSpheres = !boneSpheres.empty() && (_uint)boneSpheres.size() == boneCount;

		if (useBoneSpheres)
		{
			for (_uint b = 0; b < boneCount; ++b)
			{
				const _float4& sphere = boneSpheres[b];
				if (sphere.w <= 0.f)
					continue;

				const _matrix skinMatrix = XMLoadFloat4x4(&m_vCachedSkinMatrices[b]);
				const _vector center = XMVector3Transform(XMVectorSet(sphere.x, sphere.y, sphere.z, 1.f), skinMatrix);

				const _float sx = XMVectorGetX(XMVector3Length(skinMatrix.r[0]));
				const _float sy = XMVectorGetX(XMVector3Length(skinMatrix.r[1]));
				const _float sz = XMVectorGetX(XMVector3Length(skinMatrix.r[2]));
				const _float scaledRadius = sphere.w * max(sx, max(sy, sz));

				const _vector rVec = XMVectorReplicate(scaledRadius);
				minV = XMVectorMin(minV, XMVectorSubtract(center, rVec));
				maxV = XMVectorMax(maxV, XMVectorAdd(center, rVec));
				hasPoint = true;
			}
		}
		else
		{
			auto* vertices = static_cast<const VertexSkinnedBuffer*>(m_pMeshBuffer->m_pVertexSysMem);

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
				hasPoint = true;
				minV = XMVectorMin(minV, skinned);
				maxV = XMVectorMax(maxV, skinned);
			}
		}

		if (!hasPoint)
		{
			cache.animatedLocalBoundsValid = false;
			cache.animatedWorldBoundsValid = false;
			cache.meshWorldMatrixValid = false;
			return false;
		}

		XMStoreFloat3(&cache.cachedAnimatedLocalBoundsMin, minV);
		XMStoreFloat3(&cache.cachedAnimatedLocalBoundsMax, maxV);
		cache.cachedLocalBoundsPoseVersion = cache.skinningPoseVersion;
		cache.animatedLocalBoundsValid = true;
		cache.animatedWorldBoundsValid = false;
		cache.meshWorldMatrixValid = false;
	}

	CTransform* meshTransform = (m_pGameObject) ? m_pGameObject->GetTransform() : nullptr;
	const _matrix meshWorld = meshTransform ? meshTransform->GetSnapshotWorldMatrix() : XMMatrixIdentity();
	_float4x4 meshWorldMatrix = {};
	XMStoreFloat4x4(&meshWorldMatrix, meshWorld);

	if (!cache.animatedWorldBoundsValid
		|| cache.cachedWorldBoundsPoseVersion != cache.skinningPoseVersion
		|| !cache.meshWorldMatrixValid
		|| !IsSameFloat4x4(cache.cachedMeshWorldMatrix, meshWorldMatrix))
	{
		BuildWorldBoundsFromLocalAABB
		(
			cache.cachedAnimatedLocalBoundsMin,
			cache.cachedAnimatedLocalBoundsMax,
			meshWorld,
			cache.cachedAnimatedWorldBoundsMin,
			cache.cachedAnimatedWorldBoundsMax
		);
		cache.cachedWorldBoundsPoseVersion = cache.skinningPoseVersion;
		cache.cachedMeshWorldMatrix = meshWorldMatrix;
		cache.meshWorldMatrixValid = true;
		cache.animatedWorldBoundsValid = true;
	}

	_outMin = cache.cachedAnimatedWorldBoundsMin;
	_outMax = cache.cachedAnimatedWorldBoundsMax;
	return true;
}
void CSkinnedMeshRenderer::CreateBoneHierachy(const vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>& nodes, _int nodeIdx, CTransform* parentTf)
{
	const auto& n = nodes[nodeIdx];

	CGameObject* boneGO = m_pGameObject->Get_Scene()->Add_GameObject(n.name);
	CTransform* boneTf = boneGO->GetTransform();

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
	Render_WithCameraOverrideMaterial(_cam, nullptr);
}

void CSkinnedMeshRenderer::Render_WithCameraOverrideMaterial(CCamera* _cam, CMaterial* _overrideMaterial)
{
	if (!IsLODVisible())
		return;

	if (!_cam)
	{
		CDebug::LogError("Skinned MeshRenderer: No Camera assigned.");
		return;
	}

	CMaterial* renderMaterial = _overrideMaterial ? _overrideMaterial : m_pMaterial;
	if (!renderMaterial)
	{
		CDebug::LogError(L"Skinned MeshRenderer - No material assigned: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!m_pMeshBuffer)
	{
		CDebug::LogError(L"Skinned MeshRenderer - No MeshBuffer assigned :" + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (!_overrideMaterial && m_pMaterial)
		m_pMaterial->Set_IntValue(L"gObjectID", m_pGameObject->Get_UniqueID());

	vector3 cPos = _cam->GetTransform()->Get_Position();
	const _float3 camPos = cPos.toFloat3();

	const _matrix matWorld = m_pGameObject->GetTransform()->GetSnapshotWorldMatrix();
	const _matrix matView = _cam->GetViewMatrix();
	const _matrix matProj = _cam->GetProjectionMatrix();
	const _bool isMirrored = CRenderer::IsMirroredWorldMatrix(matWorld);
	ScopedRasterizerOverride mirroredRasterizer
	(
		m_pContext,
		isMirrored ? CGraphicDevice::GetInstance().Get_Rasterizer_CullBackMirrored() : nullptr
	);

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
	renderMaterial->Bind_Matrix(matWorld);
	renderMaterial->Bind_Camera(camPos, matView, matProj, boneCount);
	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

	if (IsInstancingEnabled())
		m_pMeshBuffer->Render_Instanced(GetInstanceCount());
	else
		m_pMeshBuffer->Render();
}

void CSkinnedMeshRenderer::Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatrix)
{
	if (!IsLODVisible())
		return;

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

	const _matrix matWorld = m_pGameObject->GetTransform()->GetSnapshotWorldMatrix();
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

void CSkinnedMeshRenderer::EnsureSkinningCacheEntry() const
{
	GetSkinningRuntimeCache(this);
}

void CSkinnedMeshRenderer::ComputeSkinning() const
{
	TryUpdateSkinningCache(nullptr);
}



