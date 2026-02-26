#pragma once

#include "Component.h"
#include <Jolt/Physics/Body/BodyID.h>

NS_BEGIN(Engine)

class ENGINE_DLL CCloth final : public CComponent
{
	friend class CGameObject;

protected:
	explicit CCloth();
	~CCloth();

private:
	static CCloth* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void FixedUpdate() override;
	void OnDestroy() override;

public:
	void SetTexturePath(const wstring& _path);
	const wstring& GetTexturePath() const;
	void RebuildClothBody();
	_bool GetUseGravity() const;
	void SetUseGravity(const _bool _useGravity);
	void AddPinnedTransform(class CTransform* _transform);
	void RemovePinnedTransform(const _uint _index);
	void ClearPinnedTransforms();
	_uint GetPinnedTransformCount() const;
	class CTransform* GetPinnedTransform(const _uint _index) const;

private:
	class CMaterial* FindTargetMaterial();
	void ApplyTextureToRenderer();
	void CreateSoftBody();
	void DestroySoftBody();
	_bool HasRendererTarget() const;
	_bool BuildClothRenderVerticesFromSoftBody(vector<VertexTexNormalTangentBuffer>& _outVertices);
	void ApplyGravityToSoftBody();
	void ApplyPinnedTransformsToSoftBody();

private:
	JPH::BodyID m_iSoftBodyID;
	_bool m_bHasSoftBody;
	_bool m_bPendingCreate;

	_uint m_iGridWidth;
	_uint m_iGridHeight;
	_float m_fGridSpacing;
	_float m_fTotalMass;
	_float m_fDamping;
	_float m_fCompliance;
	_bool m_bUseGravity;
	_bool m_bHasLastSyncedPosition;
	vector3 m_vLastSyncedPosition;
	wstring m_strTexturePath;
	vector<class CTransform*> m_vPinnedTransforms;
	vector<_uint> m_vPinnedVertexIndices;
};

NS_END
