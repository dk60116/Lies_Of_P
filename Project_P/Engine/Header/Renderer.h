#pragma once

#include "Component.h"
#include "Light.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRenderer abstract : public CComponent
{
protected:	
	explicit CRenderer();
	~CRenderer();
	
public:
	void Update() override;
	void OnDestroy() override;

protected:
	HRESULT Initialize() override;

public:
	virtual void Render_WithCamera(class CCamera* _cam) PURE;
	virtual void Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatrix) PURE;
	virtual void Render_Outline(CCamera* _cam) PURE;

public:
	CMaterial* Get_Material();
	void Set_Material(CMaterial* _material);
	virtual CMeshBuffer* Get_MeshBuffer() PURE;

public:
	const _bool IsCastShadow() const;
	virtual const _float GetScaleFactor() const PURE;
	void SetCastShadow(const _bool _on);

public:
	void CreateMeshInstancing(const _uint _count);
	void SetInstancingPosition(const _uint _index, const vector3& _pos);
	void SetInstancingRotation(const _uint _index, const vector3& _rot);
	void SetInstancingSize(const _uint _index, const vector3& _size);

	const _bool IsInstancingEnabled() const;
	const _uint GetInstanceCount() const;

protected:
	void Bind_InstanceBuffer(const _matrix& _baseWorld);
	_bool TryBindCachedStaticMatrix(const _matrix& _world);

protected:
	CMaterial* m_pMaterial;
	CMaterial* m_pOutlineMat;

	_float m_fSclaeFactor;
	_bool m_bCastShadow;

	_bool m_bUseInstancing;
	_uint m_iInstanceCount;
	ID3D11Buffer* m_pInstanceBuffer;
	ID3D11Buffer* m_pStaticMatrixBuffer;
	_bool m_bStaticMatrixUploaded;

	struct InstanceTransform
	{
		vector3 position = vector3::zero();
		vector3 rotation = vector3::zero();
		vector3 scale = vector3::one();
	};
	vector<InstanceTransform> m_vInstanceTransforms;
};

NS_END
