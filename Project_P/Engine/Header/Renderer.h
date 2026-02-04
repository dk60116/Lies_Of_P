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

protected:
	CMaterial* m_pMaterial;
	CMaterial* m_pOutlineMat;

	_float m_fSclaeFactor;
	_bool m_bCastShadow;
};

NS_END

