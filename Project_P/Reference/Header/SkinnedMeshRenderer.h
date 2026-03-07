#pragma once

#include "Renderer.h"
#include "SkinnedMeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSkinnedMeshRenderer : public CRenderer
{
	friend class CGameObject;
	friend class CAnimator;

private:
	CSkinnedMeshRenderer();
	~CSkinnedMeshRenderer();

private:
	static CSkinnedMeshRenderer* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void OnPreCull() override;
	void OnPreRender() override;
	void Render_Editor() override;
	void Render() override;
	void OnPostRender() override;
	void OnDestroy()override;

public:
	const _uint Get_BoneCount() const;
	const wstring Get_BoneName(const _uint _index) const;
	CTransform* Get_BoneTransform(const _uint _index) const;
	const _float4x4& Get_BoneOffsetMatrix(const _uint _index) const;
	_bool TryGetAnimatedWorldBounds(_float3& _outMin, _float3& _outMax) const;

	void CreateBoneHierachy(const vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>& nodes, _int nodeIdx, CTransform* parentTf);

public:
	void Render_WithCamera(CCamera* _cam) override;
	void Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatri) override;
	void Render_Outline(CCamera* _cam) override;

public:
	CMeshBuffer* Get_MeshBuffer() override;
	CSkinnedMeshBuffer* Get_SkinnedMeshBuffer();
	void Set_MeshBuffer(CSkinnedMeshBuffer* _Mesh);
	void Set_Bones(const vector<CTransform*>& _bones, vector<CTransform*>);
	vector<CTransform*>& GetRootBons();
	const wstring Get_RootBoneName(const _int _index) const;
	void AddRootBone(CTransform* _tf);

public:
	const _float GetScaleFactor() const override;

private:
	uint64_t ComputeSkinningPoseHash(_uint _boneCount) const;
	_bool TryUpdateSkinningCache(_uint* _outBoneCount = nullptr) const;
	_bool UploadBoneMatricesFromCache() const;
	void InvalidateSkinningCache() const;

private:
	CSkinnedMeshBuffer* m_pMeshBuffer;
	vector<CTransform*> m_vBones;
	vector<CTransform*> m_vRootBone;

	ID3D11Buffer* m_pBoneMatrixBuffer;
	mutable _bool m_bSkinningCacheValid;
	mutable _uint m_iCachedBoneCount;
	mutable uint64_t m_iCachedPoseHash;
	mutable vector<_float4x4> m_vCachedSkinMatrices;
	mutable vector<_float4x4> m_vCachedBoneMatrices;
};

NS_END