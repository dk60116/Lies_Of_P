#pragma once

#include "Component.h"
#include "Physics.h"
#include <array>
#include <memory>
#include <unordered_map>

NS_BEGIN(Engine)

class CRenderer;

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
	enum class ClearFlags { Skybox, SolidColor, DepthOnly, DontClear };
	enum class ViewMode { Perspective, Orthographic };
	struct RenderStats
	{
		_uint batches = 0u;
		_uint tris = 0u;
		_uint verts = 0u;
		_uint visibleSkinnedMeshes = 0u;
	};

protected:
	struct OctreeEntry;
	struct OctreeNode;

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
	_matrix GetViewMatrix() const;
	_matrix GetProjectionMatrix() const;
	const ClearFlags GetClearFlags() const;
	void SetClearFlags(const ClearFlags _flag);
	const ViewMode GetViewMode() const;
	void SetViewMode(const ViewMode _mode);
	const _uint GetCullingMask() const;
	void SetCullingMask(const _uint _mask);
	const _float GetAspect() const;
	const _float GetNear() const;
	void SetNear(const _float _value);
	const _float GetFar() const;
	void SetFar(const _float _value);
	const _float GetFieldOfView() const;
	void SetFieldOfView(const _float _value);
	const _float GetOrthographicSize() const;
	void SetOrthographicSize(const _float _value);
	const ColorValue& Get_BackgroundColor() const;
	void SetBackgroundColor(const ColorValue& _color);
	const RenderStats& GetRenderStats() const;

	void Add_RenderTarget_Mesh(class CRenderer* _mesh);
	void Add_RenderTarget_UI(class CUI* _ui);

protected:
	void Bind_ViewMatrix();
	void Bind_ProjectionMatrix();
	void Update_WorldFrustum();
	void Collect_VisibleRenderers();
	_bool IsRendererVisible(class CRenderer* _renderer) const;
	_bool TryBuildRendererWorldAABB(class CRenderer* _renderer, BoundingBox& _outAABB, _bool* _outChanged = nullptr) const;
	void SortTransparentRenderersByCameraDistance(vector<CRenderer*>& _renderers);
	void BuildStaticOctree();
	void InsertStaticOctreeEntry(OctreeNode* _node, const OctreeEntry& _entry);
	void QueryStaticOctree(const OctreeNode* _node, vector<CRenderer*>& _outVisible) const;
	_bool IsOctreeNodeLeaf(const OctreeNode* _node) const;

public:
	void RenderMesh();
	void RenderUI();
	void RenderDisplay();

public:
	void RenderObjectIDPass(const D3D11_VIEWPORT* vp);
	void RenderLightingPass_ToDiffuse(const D3D11_VIEWPORT* vp);
	void RenderLightingPass_ToSpecular(const D3D11_VIEWPORT* vp);
	void RenderLightingCombined(const D3D11_VIEWPORT* vp);
	void RenderShadowDepthPass(const D3D11_VIEWPORT* vp);
	void RenderShadowMaskPass(const D3D11_VIEWPORT* vp);
	void RenderRTDebugDisplay(const _bool _renderingEditorPass);

	void RenderCombine(const D3D11_VIEWPORT* vp);

public:
	const _int GetColorPickingID(const vector2Int& _mousePos);

	CPhysics::Ray ScreenPointToRay(const vector2Int& _pixel, _float _maxDist = 999999.f);
	CPhysics::Ray ScreenPointToRay_Editor(const vector2Int& _pixel, _float _maxDist = 999999.f);

private:
	const _bool EnsurePickStaging();

private:
	CMaterial* Add_RectMaterial(const CRenderTarget::RTType _type, const wstring& _path);
	CMaterial* Find_RectMaterial(const CRenderTarget::RTType _type);
	const D3D11_VIEWPORT* ResolveViewport() const;

protected:
	void Find_MainLight();
	void ResetRenderStats();
	void AccumulateRenderStats(class CRenderer* _renderer, _uint _instanceCount = 1u);

protected:
	ClearFlags m_eClearFlag;
	ViewMode m_eCamViewMode;
	_float4x4 m_vViewMatrix, m_vProjMatrix, m_vVPInverseMatrix;

	_float m_fAspect;
	ColorValue m_vBackgroundColor;
	_uint m_iCullingMask;
	_float m_fNear, m_fFar;
	_float m_fFieldOfView;
	_float m_fSize;

	vector<CRenderer*> m_vStaticMeshList;
	vector<CRenderer*> m_vDynamicMeshList;
	vector<CRenderer*> m_vVisibleStaticMeshList;
	vector<CRenderer*> m_vVisibleStaticMeshList_Transparent;
	vector<CRenderer*> m_vVisibleDynamicMeshList;
	vector<CRenderer*> m_vVisibleDynamicMeshList_Transparent;
	vector<CUI*> m_vUIList;
	BoundingFrustum m_sWorldFrustum;
	BoundingOrientedBox m_sWorldOrthoBounds;
	_bool m_bUseOrthographicCulling;

	struct OctreeEntry
	{
		CRenderer* renderer = nullptr;
		BoundingBox worldAABB = {};
	};

	struct OctreeNode
	{
		BoundingBox bounds = {};
		vector<OctreeEntry> entries = {};
		array<unique_ptr<OctreeNode>, 8> children = {};
		_int depth = 0;
	};

	struct RendererBoundsCache
	{
		BoundingBox worldAABB = {};
		_float4x4 worldMatrix = {};
		CMeshBuffer* meshBuffer = nullptr;
		_bool valid = false;
	};

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

	ID3D11Texture2D* m_pPickStaging;

	unique_ptr<OctreeNode> m_pStaticOctreeRoot;
	vector<CRenderer*> m_vStaticOctreeRenderers;
	mutable unordered_map<CRenderer*, RendererBoundsCache> m_mRendererBoundsCache;
	_int m_iOctreeMaxDepth;
	_int m_iOctreeMaxEntriesPerNode;
	RenderStats m_sRenderStats;

	_bool m_bIsEditor;
};

NS_END





