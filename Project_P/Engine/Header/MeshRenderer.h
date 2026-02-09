#pragma once

#include "Renderer.h"
#include "MeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMeshRenderer final : public CRenderer
{
	friend class CGameObject;

protected:
	explicit CMeshRenderer();
	~CMeshRenderer();

private:
	static CMeshRenderer* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void OnPreCull() override;
	void OnPreRender() override;
	void Render_Editor() override;
	void Render() override;
	void OnPostRender() override;

	void OnDestroy() override;

public:
	void Render_WithCamera(CCamera* _cam) override;
	void Render_ShadowDepth(CMaterial* _shadowDepthMat, const CLight::ShadowMatrices& _shadowMatrix) override;
	void Render_Outline(CCamera* _cam) override;

public:
	void CreateMeshInstancing(_uint _count);
	void SetInstancingPosition(_uint _index, const vector3& _position);
	void SetInstancingRotation(_uint _index, const vector3& _rotation);
	void SetInstancingSize(_uint _index, const vector3& _size);

	CMeshFilter* Get_MeshFilter();
	CMeshBuffer* Get_MeshBuffer() override;

	const _float GetScaleFactor() const override;

private:
	struct InstanceTransform
	{
		vector3 position;
		vector3 rotation;
		vector3 scale;
	};

	void UpdateInstanceBuffer(const _matrix& _baseWorld);
	void CreateInstanceBuffer();
	_matrix BuildInstanceWorld(const InstanceTransform& _transform) const;

	CMeshFilter* m_pMeshFilter;
	vector<InstanceTransform> m_vInstanceTransforms;
	ID3D11Buffer* m_pInstanceBuffer;
	_uint m_iInstanceCount;
	_bool m_bInstancing;
};

NS_END
