#include "epch.h"
#include "SkinnedMeshRenderer.h"
#include "EditorCamera.h"

CSkinnedMeshRenderer::CSkinnedMeshRenderer()
	: CRenderer{}
	, m_pMeshBuffer(nullptr)
	, m_vBones({})
	, m_pRootBone(nullptr)
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

	clone->m_pMeshBuffer = this->m_pMeshBuffer;
	if (clone->m_pMeshBuffer)
	if (clone->m_pMeshBuffer)
		clone->m_pMeshBuffer->AddRef();

	clone->m_vBones.clear();
	clone->m_vBones.reserve(this->m_vBones.size());
	for (auto* b : this->m_vBones)
	{
		if (b) b->AddRef();
		clone->m_vBones.push_back(b);
	}

	clone->m_pRootBone = this->m_pRootBone;
	if (clone->m_pRootBone)
		clone->m_pRootBone->AddRef();

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

	CCamera* editorCam = CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera();
	if (!editorCam)
		return E_FAIL;

	editorCam->Add_RenderTarget_Mesh(this);
	Render_Outline(editorCam);
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(_matrix) * MAX_BONE;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
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
	Render_WithCamera(CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera());
	Render_Outline(CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera());
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

		// 현재 본 월드
		_matrix boneWorld = m_vBones[i]->Get_WorldMatrix();

		// 역 바인드 포즈(Offset)
		// (m_vBoneOffsetMatrices의 인덱스가 m_vBones와 동일한 순서라는 전제)
		_matrix invBindPose = XMMatrixIdentity();
		invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]);

		// bone을 mesh local로 변환
		// (boneWorld * meshWorldInv) : boneWorld → meshLocal
		_matrix boneMeshLocal = boneWorld * meshWorldInv;

		// 최종 본 행렬
		// (invBindPose * currentBone) 형태 유지
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

	// 6) Material bind (boneCount는 클램프한 값으로)
	m_pMaterial->Bind_Matrix(matWorld);
	m_pMaterial->Bind_Camera(camPos, matView, matProj, boneCount);

	// 7) Bones CB bind (b3)
	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

	// 8) Draw
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

	// 1) World
	_matrix matWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();

	// 2) Light View/Proj
	_matrix matView = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.view));
	_matrix matProj = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&_shadowMatrix.proj));

	// Bone Count Clamp
	const _uint boneCount = min<_uint>(static_cast<_uint>(m_vBones.size()), MAX_BONE);

	//Bone Matrices
	_matrix boneMatrices[MAX_BONE];
	for (int i = 0; i < MAX_BONE; ++i)
		boneMatrices[i] = XMMatrixIdentity();

	// meshWorldInv 1회 계산
	_matrix meshWorldInv = XMMatrixIdentity();
	{
		if (m_pGameObject && m_pGameObject->Get_Transform())
		{
			_matrix meshWorld = m_pGameObject->Get_Transform()->Get_WorldMatrix();
			meshWorldInv = XMMatrixInverse(nullptr, meshWorld);
		}
	}

	// 5) 본 행렬 계산(기존 Render_WithCamera와 동일한 규칙 유지)
	for (_uint i = 0; i < boneCount; ++i)
	{
		if (!m_vBones[i])
			continue;

		_matrix boneWorld = m_vBones[i]->Get_WorldMatrix();
		_matrix invBindPose = XMLoadFloat4x4(&m_pMeshBuffer->m_vBoneOffsetMatrices[i]); // offset

		_matrix boneMeshLocal = boneWorld * meshWorldInv;

		// 기존 렌더에서 transpose해서 올렸으므로, shadow에서도 동일하게 유지
		boneMatrices[i] = XMMatrixTranspose(invBindPose * boneMeshLocal);
	}

	// 6) Bone CB 업로드
	D3D11_MAPPED_SUBRESOURCE mappedRes = {};
	HRESULT hr = m_pContext->Map(m_pBoneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	if (FAILED(hr))
	{
		CDebug::LogError(L"SkinnedMeshRenderer::Render_ShadowDepth - Failed Map BoneMatrixBuffer: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	memcpy(mappedRes.pData, boneMatrices, sizeof(_matrix) * MAX_BONE);
	m_pContext->Unmap(m_pBoneMatrixBuffer, 0);

	// 7) Material bind
	_float3 dummyPos = { 0.f, 0.f, 0.f };
	_shadowDepthMat->Bind_Matrix(matWorld);
	_shadowDepthMat->Bind_Camera(dummyPos, matView, matProj, boneCount);

	// 8) Bones CB bind (b3)
	m_pContext->VSSetConstantBuffers(3, 1, &m_pBoneMatrixBuffer);

	// 9) Draw
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

void CSkinnedMeshRenderer::Set_Bones(const vector<CTransform*>& _bones, CTransform* _rootBone)
{
	for (auto* t : m_vBones)
		Safe_Release(t);

	m_vBones.clear();

	Safe_Release(m_pRootBone);
	m_pRootBone = nullptr;

	m_vBones.reserve(_bones.size());
	for (auto* t : _bones)
	{
		if (t) t->AddRef();
		m_vBones.push_back(t);
	}

	if (_rootBone)
		m_pRootBone = _rootBone;
}

const wstring CSkinnedMeshRenderer::Get_RootBoneName() const
{
	return m_pRootBone->Get_GameObject()->Get_ObjectName();
}
