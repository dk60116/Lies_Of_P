#pragma once

#include "Component.h"
#include "Physics.h"

NS_BEGIN(Engine)

class ENGINE_DLL CCamera : public CComponent
{
	friend class CGameObject;

	struct RTDebugDisplay
	{
		CRenderTarget::RTType type;
		CMeshBuffer* quad;
		CMaterial* material;
	};

	struct InvViewProjCB
	{
		_float4x4 gInvViewProj;
	};

	struct SpecularParamsCB
	{
		_float3 camPosW; 
		_float  smoothness;

		_float  specularScale;
		_float3 pad;
	};

public:
	enum ViewMode { PERSPECTIVE, ORTHOGRAPHIC };

protected:
	explicit CCamera();
	~CCamera();

private:
	static CCamera* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void Render() override;
	void OnPostRender() override;
	void OnDestroy() override;

public:
	_matrix Get_ViewMatrix() const;
	_matrix Get_ProjectionMatrix() const;
	const ViewMode Get_ViewMode() const;
	void Set_ViewMode(const ViewMode _mode);
	void SetNear(const _float _value);
	void SetFar(const _float _value);
	const ColorValue& Get_BackgroundColor() const;
	void Set_BackgroundColor(const ColorValue& _color);

	void Add_RenderTarget_Mesh(class CRenderer* _mesh);
	void Add_RenderTarget_UI(class CUI* _ui);

protected:
	void Bind_ViewMatrix();
	void Bind_ProjectionMatrix();

public:
	void RenderMesh();
	void RenderUI();
	void RenderDisplay();

public:
	void RenderLightingPass_ToShading(const D3D11_VIEWPORT* vp);
	void RenderLightingPass_ToSpecular(const D3D11_VIEWPORT* vp);
	void RenderShadowDepthPass(const D3D11_VIEWPORT* vp);
	void RenderShadowMaskPass(const D3D11_VIEWPORT* vp);
	void RenderRTDebugDisplay();

	void RenderCombine(const D3D11_VIEWPORT* vp);

public:
	CPhysics::Ray ScreenPointToRay(const vector2Int& _pixel, _float _maxDist = 999999.f);
	CPhysics::Ray ScreenPointToRay_Editor(const vector2Int& _pixel, _float _maxDist = 999999.f);

private:
	CMaterial* Add_RectMaterial(const CRenderTarget::RTType _type, const wstring& _path);
	CMaterial* Find_RectMaterial(const CRenderTarget::RTType _type);

protected:
	void Find_MainLight();

protected:
	ViewMode m_eCamViewMode;
	_float4x4 m_vViewMatrix, m_vProjMatrix, m_vVPInverseMatrix;

	_float m_fAspect;
	ColorValue m_vBackgroundColor;
	_float m_fNear, m_fFar;
	_float m_fFieldOfView;
	_float m_fSize;

	vector<CRenderer*> m_vMeshList;
	vector<CUI*> m_vUIList;

private:
	static const ColorValue s_vDefaultCameraColor;

private:
	map<CRenderTarget::RTType, RTDebugDisplay> m_mRTDebugDisplays;

	CMeshBuffer* m_pRectBuffer;
	map<CRenderTarget::RTType, CMaterial*> m_mRectMats;

	ID3D11DepthStencilState* m_pRTDebugDS;
	ID3D11DepthStencilState* m_pRTShadowDepthDS;
	ID3D11RasterizerState* m_pRTDebugRS;
	ID3D11RasterizerState* m_pRTShdowDepthRS;
	ID3D11BlendState* m_pRTDebugBS;

	ID3D11Buffer* m_pInvViewProjCB;
	ID3D11Buffer* m_pShadowCB;

	CLight* m_pMainLight;
	CLight::ShadowMatrices m_sMainLightMatrix;
};

NS_END