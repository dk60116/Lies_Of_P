#pragma once

#include "Component.h"
#include "MeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMeshFilter : public CComponent
{
	friend class CGameObject;

protected:
	explicit CMeshFilter();
	~CMeshFilter();

private:
	static CMeshFilter* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void OnDestroy();

public:
	CMeshBuffer* Get_MeshBuffer() const;
	void Set_MeshBuffer(CMeshBuffer* _buffer);
	const _float GetScaleFactor() const;
	void SetScaleFactor(const _float _value);
	vector3 GetRotationFactor() const;
	void SetRotationFactor(const vector3& _value);

private:
	CMeshBuffer* m_pMeshBuffer;
};

NS_END

