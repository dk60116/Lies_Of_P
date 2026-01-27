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

protected:
	CMaterial* m_pMaterial;
	CMaterial* m_pOutlineMat;
};

NS_END

