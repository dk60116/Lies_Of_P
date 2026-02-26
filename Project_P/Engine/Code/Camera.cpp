#include "epch.h"
#include "Camera.h"
#include "SkinnedMeshRenderer.h"

const ColorValue CCamera::s_vDefaultCameraColor = ColorValue(49, 77, 121, 255);

CCamera::CCamera()
	: m_eCamViewMode()
	, m_vViewMatrix()
	, m_vProjMatrix()
	, m_vVPInverseMatrix()
	, m_vBackgroundColor(s_vDefaultCameraColor)
	, m_fAspect(1.f)
	, m_fNear(0.1f)
	, m_fFar(600.f)
	, m_fFieldOfView(60.f)
	, m_fSize(5.f)
	, m_vStaticMeshList({})
	, m_vDynamicMeshList({})
	, m_vVisibleStaticMeshList({})
	, m_vVisibleDynamicMeshList({})
	, m_vUIList({})
	, m_sWorldFrustum()
	, m_mRTDebugDisplays({})
	, m_pRectBuffer(nullptr)
	, m_mRectMats({})
	, m_pRTDebugDS(nullptr)
	, m_pRTShadowDepthDS(nullptr)
	, m_pRTDebugRS(nullptr)
	, m_pRTShdowDepthRS(nullptr)
	, m_pRTDebugBS(nullptr)
	, m_pInvViewProjCB(nullptr)
	, m_pShadowCB(nullptr)
	, m_pMainLight(nullptr)
	, m_sMainLightMatrix()
	, m_pPickStaging(nullptr)
	, m_pStaticOctreeRoot(nullptr)
	, m_iOctreeMaxDepth(4)
	, m_iOctreeMaxEntriesPerNode(16)
	, m_bIsEditor(false)
{
	m_strName = L"Camera";
}

CCamera::~CCamera()
{
}

CCamera* CCamera::Create()
{
	return new CCamera();
}

CComponent* CCamera::Clone() const
{
	CCamera* clone = new CCamera();

	clone->m_eCamViewMode = this->m_eCamViewMode;
	clone->m_vBackgroundColor = s_vDefaultCameraColor;
	clone->m_fAspect = this->m_fAspect;
	clone->m_fNear = this->m_fNear;
	clone->m_fFar = this->m_fFar;
	clone->m_fFieldOfView = this->m_fFieldOfView;
	clone->m_fSize = this->m_fSize;

	return clone;
}

HRESULT CCamera::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	m_bIsEditor = dynamic_cast<CEditorCamera*>(this);

	m_pRectBuffer = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Rect (Mesh Buffer)");
	if (!m_pRectBuffer)
	{
		CDebug::LogError("Not found Rect (Mesh Buffer)");
		return E_FAIL;
	}

	CMaterial* presentMat = Add_RectMaterial(CRenderTarget::RTType::Present, L"DeferredPresent (Material)");
	CMaterial* ObjectPresentMat = Add_RectMaterial(CRenderTarget::RTType::ObjectPresent, L"ObjectIDPresent (Material)");
	CMaterial* depthPresentMat = Add_RectMaterial(CRenderTarget::RTType::Depth, L"DepthPresent (Material)");
	CMaterial* shadowDepthPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepthPresent, L"ShadowDepthPresent (Material)");
	CMaterial* shadowMaskPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMaskPresent, L"ShadowMaskPresent (Material)");
	CMaterial* combineMat = Add_RectMaterial(CRenderTarget::RTType::Combine, L"DeferredCombine (Material)");
	CMaterial* shadowDepthMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepth, L"ShadowDepth (Material)");
	CMaterial* diffuseMat = Add_RectMaterial(CRenderTarget::RTType::Diffuse, L"DeferredDiffuse (Material)");
	CMaterial* specularMat = Add_RectMaterial(CRenderTarget::RTType::Specular, L"DeferredSpecular (Material)");
	CMaterial* shadowMaskMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMask, L"ShadowMask (Material)");

	auto pushDisplay = [&](CRenderTarget::RTType type, CMaterial* mat)
		{
			RTDebugDisplay desc = {};
			desc.type = type;
			desc.quad = m_pRectBuffer;
			desc.material = mat;

			m_mRTDebugDisplays[type] = desc;
		};

	pushDisplay(CRenderTarget::RTType::Combine, presentMat);
	pushDisplay(CRenderTarget::RTType::Albedo, presentMat);
	pushDisplay(CRenderTarget::RTType::Object, ObjectPresentMat);
	pushDisplay(CRenderTarget::RTType::Normal, presentMat);
	pushDisplay(CRenderTarget::RTType::Material, presentMat);
	pushDisplay(CRenderTarget::RTType::Depth, depthPresentMat);
	pushDisplay(CRenderTarget::RTType::ShadowDepth, shadowDepthPresentMat);
	pushDisplay(CRenderTarget::RTType::Diffuse, presentMat);
	pushDisplay(CRenderTarget::RTType::Specular, presentMat);
	pushDisplay(CRenderTarget::RTType::ShadowMask, shadowMaskPresentMat);

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return E_FAIL;

	{
		D3D11_DEPTH_STENCIL_DESC ds = {};
		ds.DepthEnable = FALSE;
		ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
		ds.StencilEnable = FALSE;

		if (FAILED(device->CreateDepthStencilState(&ds, &m_pRTDebugDS)))
			return E_FAIL;
	}

	{
		D3D11_DEPTH_STENCIL_DESC sds = {};
		sds.DepthEnable = TRUE;
		sds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		sds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		sds.StencilEnable = FALSE;
		device->CreateDepthStencilState(&sds, &m_pRTShadowDepthDS);
	}

	{
		D3D11_RASTERIZER_DESC rs = {};
		rs.FillMode = D3D11_FILL_SOLID;
		rs.CullMode = D3D11_CULL_NONE;
		rs.FrontCounterClockwise = FALSE;
		rs.DepthClipEnable = TRUE;

		if (FAILED(device->CreateRasterizerState(&rs, &m_pRTDebugRS)))
			return E_FAIL;
	}

	{
		D3D11_RASTERIZER_DESC srs = {};
		srs.FillMode = D3D11_FILL_SOLID;
		srs.CullMode = D3D11_CULL_FRONT;
		srs.FrontCounterClockwise = FALSE;
		srs.DepthClipEnable = TRUE;
		srs.DepthBias = 1000;
		srs.SlopeScaledDepthBias = 1.0f;
		srs.DepthBiasClamp = 0.f;

		if (FAILED(device->CreateRasterizerState(&srs, &m_pRTShdowDepthRS)))
			return E_FAIL;
	}

	{
		D3D11_BLEND_DESC bs = {};
		bs.AlphaToCoverageEnable = FALSE;
		bs.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC rt = {};
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		bs.RenderTarget[0] = rt;

		if (FAILED(device->CreateBlendState(&bs, &m_pRTDebugBS)))
			return E_FAIL;
	}

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(InvViewProjCB);
	if (FAILED(device->CreateBuffer(&bd, nullptr, &m_pInvViewProjCB)))
		return E_FAIL;
	m_pInvViewProjCB->AddRef();

	D3D11_BUFFER_DESC sbd = {};
	sbd.Usage = D3D11_USAGE_DEFAULT;
	sbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	sbd.ByteWidth = sizeof(ShadowCB);
	if (FAILED(device->CreateBuffer(&sbd, nullptr, &m_pShadowCB)))
		return E_FAIL;
	m_pShadowCB->AddRef();

	return S_OK;
}

void CCamera::Update()
{
	const D3D11_VIEWPORT* vp = ResolveViewport();

	if (vp && vp->Height > 0.f)
		m_fAspect = vp->Width / vp->Height;
	else
	{
		vector2Int res = m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution() : CDisplay::GetInstance().Get_ScreenResolution();
		m_fAspect = (res.y > 0) ? (_float)res.x / (_float)res.y : 1.f;
	}

	Find_MainLight();

	if (m_pMainLight)
		m_pMainLight->BuildDirectionalShadow(this, CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().directionalLightShadowDist, m_sMainLightMatrix);

	Bind_ViewMatrix();
	Bind_ProjectionMatrix();
	Update_WorldFrustum();
}

void CCamera::Render()
{
}

void CCamera::OnPostRender()
{
	m_vStaticMeshList.clear();
	m_vDynamicMeshList.clear();
	m_vVisibleStaticMeshList.clear();
	m_vVisibleStaticMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.clear();
	m_vVisibleDynamicMeshList_Transparent.clear();
}

void CCamera::OnDestroy()
{
	m_mRectMats.clear();

	m_mRTDebugDisplays.clear();

	Safe_Release(m_pRTDebugDS);
	Safe_Release(m_pRTShadowDepthDS);
	Safe_Release(m_pRTDebugRS);
	Safe_Release(m_pRTShdowDepthRS);
	Safe_Release(m_pRTDebugBS);

	Safe_Release(m_pInvViewProjCB);
	Safe_Release(m_pShadowCB);

	Safe_Release(m_pPickStaging);
	Safe_Release(m_pMainLight);

	if (auto scene = CSceneManager::GetInstance().Get_CrtScene())
		scene->Remove_Camera(this);
}

_matrix CCamera::Get_ViewMatrix() const
{
	_matrix result = XMLoadFloat4x4(&m_vViewMatrix);
	return result;
}

_matrix CCamera::Get_ProjectionMatrix() const
{
	_matrix result = XMLoadFloat4x4(&m_vProjMatrix);
	return result;
}

const CCamera::ViewMode CCamera::Get_ViewMode() const
{
	return m_eCamViewMode;
}

void CCamera::Set_ViewMode(const ViewMode _mode)
{
	m_eCamViewMode = _mode;
}

void CCamera::SetNear(const _float _value)
{
	m_fNear = _value;
}

void CCamera::SetFar(const _float _value)
{
	m_fFar = _value;
}

const ColorValue& CCamera::Get_BackgroundColor() const
{
	return m_vBackgroundColor;
}

void CCamera::Set_BackgroundColor(const ColorValue& _color)
{
	m_vBackgroundColor = _color;
}

void CCamera::Add_RenderTarget_Mesh(CRenderer* _mesh)
{
	if (!_mesh || !_mesh->Get_GameObject())
		return;

	if (_mesh->Get_GameObject()->IsStatic(CGameObject::STATIC_METHOD::TransformStatic))
		m_vStaticMeshList.push_back(_mesh);
	else
		m_vDynamicMeshList.push_back(_mesh);
}

void CCamera::Add_RenderTarget_UI(CUI* _ui)
{
	m_vUIList.push_back(_ui);
}

void CCamera::Bind_ViewMatrix()
{
	_matrix inverseWorldMatrix = Get_Transform()->Get_InverseWorldMatrix();
	XMStoreFloat4x4(&m_vViewMatrix, inverseWorldMatrix);
}

void CCamera::Bind_ProjectionMatrix()
{
	switch (m_eCamViewMode)
	{
	case CCamera::PERSPECTIVE:
	{
		_matrix projMat = XMMatrixPerspectiveFovLH
		(
			XMConvertToRadians(m_fFieldOfView),
			m_fAspect,
			m_fNear, m_fFar
		);

		XMStoreFloat4x4(&m_vProjMatrix, projMat);
	}
	break;

	case CCamera::ORTHOGRAPHIC:
	{
		const _float fHalfHeight = m_fSize * 0.5f;
		const _float fHalfWidth = fHalfHeight * m_fAspect;

		_matrix projMat = XMMatrixOrthographicOffCenterLH
		(
			-fHalfWidth, fHalfWidth,
			-fHalfHeight, fHalfHeight,
			m_fNear, m_fFar
		);

		XMStoreFloat4x4(&m_vProjMatrix, projMat);
	}
	break;

	default:
		break;
	}

	_matrix inv = XMMatrixInverse(nullptr, Get_ViewMatrix() * Get_ProjectionMatrix());
	XMStoreFloat4x4(&m_vVPInverseMatrix, inv);
}

void CCamera::RenderMesh()
{
	Collect_VisibleRenderers();

	for (TRAVERSAL_ITER(m_vVisibleStaticMeshList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->Render_WithCamera(this);
	}

	for (TRAVERSAL_ITER(m_vVisibleDynamicMeshList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->Render_WithCamera(this);
	}

	SortTransparentRenderersByCameraDistance(m_vVisibleStaticMeshList_Transparent);
	SortTransparentRenderersByCameraDistance(m_vVisibleDynamicMeshList_Transparent);

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!context || !scene)
		return;

	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;
	context->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	const _float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	context->OMSetBlendState(scene->Get_BlendingState(), blendFactor, 0xFFFFFFFF);

	for (TRAVERSAL_ITER(m_vVisibleStaticMeshList_Transparent, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->Render_WithCamera(this);
	}

	for (TRAVERSAL_ITER(m_vVisibleDynamicMeshList_Transparent, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->Render_WithCamera(this);
	}

	context->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);
	Safe_Release(prevBS);
}


void CCamera::Update_WorldFrustum()
{
	_matrix cullingView = Get_ViewMatrix();
	_matrix cullingProj = Get_ProjectionMatrix();

	if (m_bIsEditor)
	{
		if (CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject())
		{
			if (CCamera* selectedCamera = selected->GetComponent<CCamera>())
			{
				cullingView = selectedCamera->Get_ViewMatrix();
				cullingProj = selectedCamera->Get_ProjectionMatrix();
			}
		}
	}

	BoundingFrustum localFrustum = {};
	BoundingFrustum::CreateFromMatrix(localFrustum, cullingProj);

	_matrix invView = XMMatrixInverse(nullptr, cullingView);
	m_sWorldFrustum = localFrustum;
	m_sWorldFrustum.Transform(m_sWorldFrustum, invView);
}

_bool CCamera::TryBuildRendererWorldAABB(CRenderer* _renderer, BoundingBox& _outAABB) const
{
	if (!_renderer || !_renderer->Get_GameObject())
		return false;

	CMeshBuffer* meshBuffer = _renderer->Get_MeshBuffer();
	if (!meshBuffer)
		return false;

	if (CSkinnedMeshRenderer* skinned = dynamic_cast<CSkinnedMeshRenderer*>(_renderer))
	{
		_float3 minBound = {};
		_float3 maxBound = {};
		if (skinned->TryGetAnimatedWorldBounds(minBound, maxBound))
		{
			BoundingBox::CreateFromPoints(_outAABB, XMLoadFloat3(&minBound), XMLoadFloat3(&maxBound));
			return true;
		}
	}

	const BoundingBox& localBox = meshBuffer->Get_Info().boundingBox;
	BoundingOrientedBox localObb = {};
	BoundingOrientedBox::CreateFromBoundingBox(localObb, localBox);

	_matrix world = _renderer->Get_Transform()->Get_WorldMatrix();
	BoundingOrientedBox worldObb = {};
	localObb.Transform(worldObb, world);

	XMFLOAT3 corners[8] = {};
	worldObb.GetCorners(corners);

	_vector minV = XMLoadFloat3(&corners[0]);
	_vector maxV = minV;
	for (_int i = 1; i < 8; ++i)
	{
		_vector p = XMLoadFloat3(&corners[i]);
		minV = XMVectorMin(minV, p);
		maxV = XMVectorMax(maxV, p);
	}

	BoundingBox::CreateFromPoints(_outAABB, minV, maxV);
	return true;
}

_bool CCamera::IsRendererVisible(CRenderer* _renderer) const
{
	BoundingBox worldAABB = {};
	if (!TryBuildRendererWorldAABB(_renderer, worldAABB))
		return false;

	ContainmentType contain = m_sWorldFrustum.Contains(worldAABB);
	return contain != ContainmentType::DISJOINT;
}

_bool CCamera::IsOctreeNodeLeaf(const OctreeNode* _node) const
{
	if (!_node)
		return true;

	for (const auto& child : _node->children)
	{
		if (child)
			return false;
	}
	return true;
}

void CCamera::InsertStaticOctreeEntry(OctreeNode* _node, const OctreeEntry& _entry)
{
	if (!_node)
		return;

	if (_node->depth >= m_iOctreeMaxDepth)
	{
		_node->entries.push_back(_entry);
		return;
	}

	const _float3& c = _node->bounds.Center;
	const _float3& e = _node->bounds.Extents;
	_float3 childExtent = { e.x * 0.5f, e.y * 0.5f, e.z * 0.5f };

	_int fitChild = -1;
	for (_int i = 0; i < 8; ++i)
	{
		_float3 sign =
		{
			(i & 1) ? 0.5f : -0.5f,
			(i & 2) ? 0.5f : -0.5f,
			(i & 4) ? 0.5f : -0.5f
		};
		_float3 childCenter =
		{
			c.x + e.x * sign.x,
			c.y + e.y * sign.y,
			c.z + e.z * sign.z
		};

		BoundingBox childBox = {};
		childBox.Center = childCenter;
		childBox.Extents = childExtent;

		ContainmentType contain = childBox.Contains(_entry.worldAABB);
		if (contain == ContainmentType::CONTAINS)
		{
			fitChild = i;
			break;
		}
	}

	if (fitChild < 0)
	{
		_node->entries.push_back(_entry);
		return;
	}

	if (!_node->children[fitChild])
	{
		_node->children[fitChild] = make_unique<OctreeNode>();
		_node->children[fitChild]->depth = _node->depth + 1;

		_float3 sign =
		{
			(fitChild & 1) ? 0.5f : -0.5f,
			(fitChild & 2) ? 0.5f : -0.5f,
			(fitChild & 4) ? 0.5f : -0.5f
		};
		_node->children[fitChild]->bounds.Center =
		{
			c.x + e.x * sign.x,
			c.y + e.y * sign.y,
			c.z + e.z * sign.z
		};
		_node->children[fitChild]->bounds.Extents = childExtent;
	}

	InsertStaticOctreeEntry(_node->children[fitChild].get(), _entry);
}

void CCamera::BuildStaticOctree()
{
	m_pStaticOctreeRoot.reset();
	if (m_vStaticMeshList.empty())
		return;

	vector<OctreeEntry> entries;
	entries.reserve(m_vStaticMeshList.size());

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);

	for (auto* renderer : m_vStaticMeshList)
	{
		if (!renderer || !renderer->Get_GameObject())
			continue;
		if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable())
			continue;

		BoundingBox worldAABB = {};
		if (!TryBuildRendererWorldAABB(renderer, worldAABB))
			continue;

		entries.push_back({ renderer, worldAABB });

		_vector aabbMin = XMVectorSet(worldAABB.Center.x - worldAABB.Extents.x, worldAABB.Center.y - worldAABB.Extents.y, worldAABB.Center.z - worldAABB.Extents.z, 0.f);
		_vector aabbMax = XMVectorSet(worldAABB.Center.x + worldAABB.Extents.x, worldAABB.Center.y + worldAABB.Extents.y, worldAABB.Center.z + worldAABB.Extents.z, 0.f);
		minV = XMVectorMin(minV, aabbMin);
		maxV = XMVectorMax(maxV, aabbMax);
	}

	if (entries.empty())
		return;

	BoundingBox rootBounds = {};
	BoundingBox::CreateFromPoints(rootBounds, minV, maxV);

	_float maxExtent = max(rootBounds.Extents.x, max(rootBounds.Extents.y, rootBounds.Extents.z));
	rootBounds.Extents = { maxExtent, maxExtent, maxExtent };

	m_pStaticOctreeRoot = make_unique<OctreeNode>();
	m_pStaticOctreeRoot->bounds = rootBounds;
	m_pStaticOctreeRoot->depth = 0;

	for (const auto& entry : entries)
		InsertStaticOctreeEntry(m_pStaticOctreeRoot.get(), entry);
}

void CCamera::QueryStaticOctree(const OctreeNode* _node, vector<CRenderer*>& _outVisible) const
{
	if (!_node)
		return;

	ContainmentType contain = m_sWorldFrustum.Contains(_node->bounds);
	if (contain == ContainmentType::DISJOINT)
		return;

	if (contain == ContainmentType::CONTAINS)
	{
		for (const auto& entry : _node->entries)
			_outVisible.push_back(entry.renderer);

		for (const auto& child : _node->children)
		{
			if (!child)
				continue;
			QueryStaticOctree(child.get(), _outVisible);
		}
		return;
	}

	for (const auto& entry : _node->entries)
	{
		ContainmentType entryContain = m_sWorldFrustum.Contains(entry.worldAABB);
		if (entryContain != ContainmentType::DISJOINT)
			_outVisible.push_back(entry.renderer);
	}

	for (const auto& child : _node->children)
	{
		if (!child)
			continue;
		QueryStaticOctree(child.get(), _outVisible);
	}
}

void CCamera::SortTransparentRenderersByCameraDistance(vector<CRenderer*>& _renderers) const
{
	if (_renderers.empty())
		return;

	vector3 cameraPos = Get_Transform()->Get_Position();

	sort(_renderers.begin(), _renderers.end(), [&](CRenderer* _lhs, CRenderer* _rhs)
		{
			if (!_lhs || !_rhs)
				return _lhs != nullptr;

			vector3 lhsPos = _lhs->Get_Transform()->Get_Position();
			vector3 rhsPos = _rhs->Get_Transform()->Get_Position();

			const _float lhsDistSq = (lhsPos - cameraPos).SqrMagnitude();
			const _float rhsDistSq = (rhsPos - cameraPos).SqrMagnitude();

			return lhsDistSq > rhsDistSq;
		});
}

void CCamera::Collect_VisibleRenderers()
{
	m_vVisibleStaticMeshList.clear();
	m_vVisibleStaticMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.clear();
	m_vVisibleDynamicMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.reserve(m_vDynamicMeshList.size());

	BuildStaticOctree();
	if (m_pStaticOctreeRoot)
		QueryStaticOctree(m_pStaticOctreeRoot.get(), m_vVisibleStaticMeshList);

	vector<CRenderer*> opaqueStatic = {};
	opaqueStatic.reserve(m_vVisibleStaticMeshList.size());

	for (auto* renderer : m_vVisibleStaticMeshList)
	{
		if (!renderer || !renderer->Get_Material())
			continue;

		if (!renderer->Get_Material()->IsTransparnet())
			opaqueStatic.push_back(renderer);
		else
			m_vVisibleStaticMeshList_Transparent.push_back(renderer);
	}

	m_vVisibleStaticMeshList.swap(opaqueStatic);

	for (auto* renderer : m_vDynamicMeshList)
	{
		if (!renderer || !renderer->Get_GameObject())
			continue;

		if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable())
			continue;

		if (IsRendererVisible(renderer))
		{
			if (!renderer->Get_Material()->IsTransparnet())
				m_vVisibleDynamicMeshList.push_back(renderer);
			else
				m_vVisibleDynamicMeshList_Transparent.push_back(renderer);
		}
	}
}

void CCamera::RenderUI()
{
	_matrix viewMat = XMMatrixTranslation(50.f, 50.f, 0.f);

	_vector det = {};
	const _matrix inverseMat = XMMatrixInverse(&det, viewMat);

	_float4 camPosF4;
	XMStoreFloat4(&camPosF4, inverseMat.r[3]);

	const _float aspect = CDisplay::GetInstance().Get_Aspect();
	const _float fHalfHeight = 7.2f * 0.5f;
	const _float fHalfWidth = fHalfHeight * aspect;

	const _matrix projMat = XMMatrixOrthographicOffCenterLH
	(
		-fHalfWidth, fHalfWidth,
		-fHalfHeight, fHalfHeight,
		0.f, 1.f
	);

	if (m_vUIList.size() <= 0)
		return;

	for (TRAVERSAL_ITER(m_vUIList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
		{
			if (CImage* img = dynamic_cast<CImage*>(*it))
			{
				img->Bind_UIMaterial();
				(*it)->Bind_Matrix();
				(*it)->Bind_Camera(inverseMat, projMat);
				(*it)->Bind_Mesh();
			}
			else if (CText* txt = dynamic_cast<CText*>(*it))
			{
				txt->RenderText();
			}
		}
	}

	m_vUIList.clear();
}

void CCamera::RenderDisplay()
{
	if (!m_pRectBuffer)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvCombine = rtm.GetSRV(CRenderTarget::RTType::Combine, m_bIsEditor);

	if (!srvCombine)
		return;

	CMaterial* presentMat = nullptr;
	{
		auto it = m_mRTDebugDisplays.find(CRenderTarget::RTType::Combine);
		if (it != m_mRTDebugDisplays.end())
			presentMat = it->second.material;
	}
	if (!presentMat)
		return;

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr; _float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	const D3D11_VIEWPORT* useVP = ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();
	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	const _float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = {};

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	presentMat->Bind_Matrix(w);
	presentMat->Bind_Camera(camPos, v, p, 0);

	ctx->PSSetShaderResources(0, 1, &srvCombine);
	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderRTDebugDisplay(const _bool _renderingEditorPass)
{
	if (m_bIsEditor)
	{
		if (!_renderingEditorPass)
			return;
	}
	else
	{
		if (_renderingEditorPass)
			return;

		if (CEditor::GetInstance().Get_SelectedGameObject() != m_pGameObject)
			return;
	}

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	if (!context || m_mRTDebugDisplays.empty())
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	context->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	context->RSGetState(&prevRS);
	context->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	context->RSGetViewports(&prevVPCount, &prevVP);

	if (m_pRTDebugDS)
		context->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		context->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	if (m_pRTDebugBS)
		context->OMSetBlendState(m_pRTDebugBS, bf, 0xFFFFFFFF);
	else
		context->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	vector2Int res = m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution() : CDisplay::GetInstance().Get_ScreenResolution();

	_float screenW = (_float)res.x;
	_float screenH = (_float)res.y;

	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0.f;
	vp.TopLeftY = 0.f;
	vp.Width = screenW;
	vp.Height = screenH;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	context->RSSetViewports(1, &vp);

	_matrix view = XMMatrixIdentity();
	_matrix proj = XMMatrixOrthographicOffCenterLH(0.f, screenW, screenH, 0.f, 0.f, 1.f);
	_float3 camPos = { 0.f, 0.f, -1.f };

	const D3D11_VIEWPORT* areaVP = ResolveViewport();
	if (!areaVP)
		areaVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	const _float areaX = areaVP ? areaVP->TopLeftX : 0.f;
	const _float areaY = areaVP ? areaVP->TopLeftY : 0.f;
	const _float areaW = areaVP ? areaVP->Width : screenW;
	const _float areaH = areaVP ? areaVP->Height : screenH;

	const D3D11_VIEWPORT* srcVP = ResolveViewport();
	if (!srcVP)
		srcVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	_float srcW = srcVP ? srcVP->Width : areaW;
	_float srcH = srcVP ? srcVP->Height : areaH;
	_float srcAspect = (srcH > 0.f) ? (srcW / srcH) : 1.f;

	CRenderTarget::RTType types[] =
	{
		CRenderTarget::RTType::Albedo,
		CRenderTarget::RTType::Object,
		CRenderTarget::RTType::Normal,
		CRenderTarget::RTType::Material,
		CRenderTarget::RTType::Depth,
		CRenderTarget::RTType::ShadowDepth,
		CRenderTarget::RTType::Diffuse,
		CRenderTarget::RTType::Specular,
		CRenderTarget::RTType::ShadowMask
	};

	const _int kCount = (_int)(sizeof(types) / sizeof(types[0]));
	const _int kMaxPerColumn = 5;

	const _int rightRows = min(kMaxPerColumn, kCount);
	const _int leftRows = max(0, kCount - kMaxPerColumn);
	const _int maxRows = max(rightRows, leftRows);

	const _float margin = 12.f;
	const _float gap = 10.f;

	_float rectH = 0.f;
	if (maxRows > 0)
		rectH = (areaH - margin * 2.f - (_float)(maxRows - 1) * gap) / (_float)maxRows;

	if (rectH < 1.f)
		rectH = 1.f;

	_float rectW = rectH * srcAspect;

	const _float baseY = (areaY + areaH - margin - rectH * 0.5f);

	for (_int i = 0; i < kCount; ++i)
	{
		auto it = m_mRTDebugDisplays.find(types[i]);
		if (it == m_mRTDebugDisplays.end())
			continue;

		RTDebugDisplay& disp = it->second;
		if (!disp.quad || !disp.material)
			continue;

		const _bool bRightColumn = (i < kMaxPerColumn);
		const _int row = bRightColumn ? i : (i - kMaxPerColumn);

		_float cx = bRightColumn
			? (areaX + areaW - margin - rectW * 0.5f)
			: (areaX + margin + rectW * 0.5f);

		_float cy = baseY - (_float)row * (rectH + gap);

		_matrix world = XMMatrixScaling(rectW, rectH, 1.f) * XMMatrixTranslation(cx, cy, 0.f);

		disp.material->Bind_Matrix(world);
		disp.material->Bind_Camera(camPos, view, proj, 0);

		const _bool useEditorRT = (types[i] == CRenderTarget::RTType::ShadowDepth) ? false : m_bIsEditor;
		ID3D11ShaderResourceView* srv = CRenderTargetManager::GetInstance().GetSRV(types[i], useEditorRT);

		context->PSSetShaderResources(0, 1, &srv);
		disp.quad->Render();
	}

	CRenderTargetManager::GetInstance().Unbind_AllSRVs_PS(context, m_bIsEditor);

	if (prevVPCount > 0)
		context->RSSetViewports(1, &prevVP);

	context->OMSetRenderTargets(1, &prevRTV, prevDSV);

	context->OMSetDepthStencilState(prevDS, prevStencilRef);
	context->RSSetState(prevRS);
	context->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderObjectIDPass(const D3D11_VIEWPORT* vp)
{
}

void CCamera::RenderLightingPass_ToDiffuse(const D3D11_VIEWPORT* vp)
{
	CMaterial* shadingMat = Find_RectMaterial(CRenderTarget::RTType::Diffuse);

	if (!shadingMat || !m_pRectBuffer || !m_pInvViewProjCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvMaterial = rtm.GetSRV(CRenderTarget::RTType::Material, m_bIsEditor);

	ID3D11RenderTargetView* rtvDiffuse = rtm.GetRTV(CRenderTarget::RTType::Diffuse, m_bIsEditor);

	if (!srvNormal || !srvDepth || !srvMaterial || !rtvDiffuse)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvDiffuse, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvDiffuse, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0.f;
	_float H = 0.f;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = Get_Transform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	shadingMat->Bind_Matrix(w);
	shadingMat->Bind_Camera(camPos, v, p, 0);

	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();
	shadingMat->Bind_Light(lights.empty() ? nullptr : lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[4] = { srvAlbedo, srvNormal, srvDepth, srvMaterial };
	ctx->PSSetShaderResources(0, 4, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderLightingPass_ToSpecular(const D3D11_VIEWPORT* vp)
{
	CMaterial* specularMat = Find_RectMaterial(CRenderTarget::RTType::Specular);

	if (!specularMat || !m_pRectBuffer || !m_pInvViewProjCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvMaterial = rtm.GetSRV(CRenderTarget::RTType::Material, m_bIsEditor);

	ID3D11RenderTargetView* rtvSpecular = rtm.GetRTV(CRenderTarget::RTType::Specular, m_bIsEditor);

	if (!srvAlbedo || !srvNormal || !srvDepth || !srvMaterial || !rtvSpecular)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvSpecular, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvSpecular, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0;
	_float H = 0;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	_float3 camPos = Get_Transform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	specularMat->Bind_Matrix(w);
	specularMat->Bind_Camera(camPos, v, p, 0);

	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();
	specularMat->Bind_Light(lights.empty() ? nullptr : lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[4] = { srvAlbedo, srvNormal, srvDepth, srvMaterial };
	ctx->PSSetShaderResources(0, 4, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderShadowDepthPass(const D3D11_VIEWPORT* vp)
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!device || !ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11DepthStencilView* dsvShadow = rtm.GetDSV(CRenderTarget::RTType::ShadowDepth, false);
	if (!dsvShadow)
		return;

	if (!m_pMainLight)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);
	ctx->OMSetRenderTargets(0, nullptr, dsvShadow);

	const _uint shadowSize = (_uint)CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;
	D3D11_VIEWPORT vpt = {};
	vpt.TopLeftX = 0.f;
	vpt.TopLeftY = 0.f;
	vpt.Width = (_float)shadowSize;
	vpt.Height = (_float)shadowSize;
	vpt.MinDepth = 0.f;
	vpt.MaxDepth = 1.f;
	ctx->RSSetViewports(1, &vpt);

	ctx->ClearDepthStencilView(dsvShadow, D3D11_CLEAR_DEPTH, 1.0f, 0);

	if (m_pRTShadowDepthDS)
		ctx->OMSetDepthStencilState(m_pRTShadowDepthDS, 0);
	if (m_pRTShdowDepthRS)
		ctx->RSSetState(m_pRTShdowDepthRS);

	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	CMaterial* shadowDepthMat = Find_RectMaterial(CRenderTarget::RTType::ShadowDepth);

	_matrix lightView = XMLoadFloat4x4(&m_sMainLightMatrix.view);
	const _float shadowDistance = CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().directionalLightShadowDist;
	const _float lightHalfExtent = shadowDistance * 0.5f;
	const _float lightNear = 0.0f;
	const _float lightFar = shadowDistance * 2.0f;

	auto isRenderableShadowTarget = [](CRenderer* r)
	{
		if (!r || !r->Get_GameObject())
			return false;
		if (!r->Get_GameObject()->IsRecursiveActive())
			return false;
		if (!r->Get_Enable())
			return false;
		if (!r->IsCastShadow())
			return false;
		if (!r->Get_MeshBuffer())
			return false;
		return true;
	};

	auto isShadowVisible = [this, &lightView, lightHalfExtent, lightNear, lightFar](CRenderer* r)
	{
		BoundingBox worldAABB = {};
		if (!TryBuildRendererWorldAABB(r, worldAABB))
			return false;

		XMFLOAT3 corners[8] = {};
		worldAABB.GetCorners(corners);

		_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
		_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
		for (_int i = 0; i < 8; ++i)
		{
			_vector p = XMLoadFloat3(&corners[i]);
			_vector pLS = XMVector3TransformCoord(p, lightView);
			minV = XMVectorMin(minV, pLS);
			maxV = XMVectorMax(maxV, pLS);
		}

		_float3 aabbMin = {};
		_float3 aabbMax = {};
		XMStoreFloat3(&aabbMin, minV);
		XMStoreFloat3(&aabbMax, maxV);

		if (aabbMax.x < -lightHalfExtent || aabbMin.x > lightHalfExtent)
			return false;
		if (aabbMax.y < -lightHalfExtent || aabbMin.y > lightHalfExtent)
			return false;
		if (aabbMax.z < lightNear || aabbMin.z > lightFar)
			return false;

		return true;
	};

	struct ShadowBatchKey
	{
		CMeshBuffer* meshBuffer;
		CMaterial* material;
		_bool castShadow;

		_bool operator==(const ShadowBatchKey& rhs) const
		{
			return meshBuffer == rhs.meshBuffer && material == rhs.material && castShadow == rhs.castShadow;
		}
	};

	struct ShadowBatchKeyHash
	{
		size_t operator()(const ShadowBatchKey& key) const
		{
			size_t h1 = hash<void*>()(static_cast<void*>(key.meshBuffer));
			size_t h2 = hash<void*>()(static_cast<void*>(key.material));
			size_t h3 = hash<int>()(static_cast<int>(key.castShadow));
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	unordered_map<ShadowBatchKey, vector<CRenderer*>, ShadowBatchKeyHash> staticBatches;
	staticBatches.reserve(m_vStaticMeshList.size());

	for (auto* r : m_vStaticMeshList)
	{
		if (!isRenderableShadowTarget(r))
			continue;
		if (!isShadowVisible(r))
			continue;

		ShadowBatchKey key = { r->Get_MeshBuffer(), r->Get_Material(), r->IsCastShadow() };
		staticBatches[key].push_back(r);
	}

	for (auto& kv : staticBatches)
	{
		auto& batch = kv.second;
		if (batch.empty())
			continue;

		CRenderer* leader = batch[0];
		if (!leader || !leader->Get_GameObject() || !leader->Get_Transform())
			continue;

		const _uint maxInstanceCount = 128u;
		const vector3 leaderPos = leader->Get_Transform()->Get_Position();
		const vector3 leaderRot = leader->Get_Transform()->Get_EulerAngles();
		const vector3 leaderScale = leader->Get_Transform()->Get_LocalScale();

		for (size_t offset = 0; offset < batch.size(); offset += maxInstanceCount)
		{
			const size_t remain = batch.size() - offset;
			const _uint chunkCount = static_cast<_uint>(min<size_t>(remain, maxInstanceCount));

			leader->CreateMeshInstancing(chunkCount);

			for (_uint i = 0; i < chunkCount; ++i)
			{
				CRenderer* r = batch[offset + i];
				if (!r || !r->Get_Transform())
					continue;

				const vector3 pos = r->Get_Transform()->Get_Position();
				const vector3 rot = r->Get_Transform()->Get_EulerAngles();
				const vector3 scale = r->Get_Transform()->Get_LocalScale();

				const vector3 relPos = pos - leaderPos;
				const vector3 relRot = rot - leaderRot;
				const vector3 relScale = vector3
				(
					leaderScale.x != 0.f ? scale.x / leaderScale.x : 1.f,
					leaderScale.y != 0.f ? scale.y / leaderScale.y : 1.f,
					leaderScale.z != 0.f ? scale.z / leaderScale.z : 1.f
				);

				leader->SetInstancingPosition(i, relPos);
				leader->SetInstancingRotation(i, relRot);
				leader->SetInstancingSize(i, relScale);
			}

			leader->Render_ShadowDepth(shadowDepthMat, m_sMainLightMatrix);
			leader->CreateMeshInstancing(0);
		}
	}

	for (auto* r : m_vDynamicMeshList)
	{
		if (!isRenderableShadowTarget(r))
			continue;
		if (!isShadowVisible(r))
			continue;

		r->Render_ShadowDepth(shadowDepthMat, m_sMainLightMatrix);
	}

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0) ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderShadowMaskPass(const D3D11_VIEWPORT* vp)
{
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!ctx || !m_pRectBuffer || !m_pInvViewProjCB || !m_pShadowCB)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11RenderTargetView* rtvShadowMask = rtm.GetRTV(CRenderTarget::RTType::ShadowMask, m_bIsEditor);
	ID3D11ShaderResourceView* srvSceneDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvShadowDepth = rtm.GetSRV(CRenderTarget::RTType::ShadowDepth, false);

	if (!rtvShadowMask || !srvSceneDepth || !srvShadowDepth)
		return;

	CMaterial* shadowMaskMat = Find_RectMaterial(CRenderTarget::RTType::ShadowMask);
	if (!shadowMaskMat)
		return;

	if (!m_pMainLight)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP{};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);
	ctx->OMSetRenderTargets(1, &rtvShadowMask, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 1.f, 1.f, 1.f, 1.f };
	ctx->ClearRenderTargetView(rtvShadowMask, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0;
	_float H = 0;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = Get_Transform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	_matrix lv = XMLoadFloat4x4(&m_sMainLightMatrix.view);
	_matrix lp = XMLoadFloat4x4(&m_sMainLightMatrix.proj);

	_matrix lightVP = XMMatrixMultiply(lv, lp);

	ShadowCB scb = {};
	XMStoreFloat4x4(&scb.shadowViewProj, lightVP);

	const _float shadowSize = (_float)CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;
	scb.invShadowMapSize = _float2(1.0f / shadowSize, 1.0f / shadowSize);

	scb.bias = CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().shadowBias;

	ctx->UpdateSubresource(m_pShadowCB, 0, nullptr, &scb, 0, 0);
	ctx->PSSetConstantBuffers(6, 1, &m_pShadowCB);

	shadowMaskMat->Bind_Matrix(w);
	shadowMaskMat->Bind_Camera(camPos, v, p, 0);

	ID3D11ShaderResourceView* srvs[2] = { srvSceneDepth, srvShadowDepth };
	ctx->PSSetShaderResources(0, 2, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderCombine(const D3D11_VIEWPORT* vp)
{
	if (!m_pRectBuffer)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvDiffuse = rtm.GetSRV(CRenderTarget::RTType::Diffuse, m_bIsEditor);
	ID3D11ShaderResourceView* srvSpecular = rtm.GetSRV(CRenderTarget::RTType::Specular, m_bIsEditor);
	ID3D11ShaderResourceView* srvShadow = rtm.GetSRV(CRenderTarget::RTType::ShadowMask, m_bIsEditor);

	ID3D11RenderTargetView* rtvCombine = rtm.GetRTV(CRenderTarget::RTType::Combine, m_bIsEditor);

	if (!srvDepth || !srvAlbedo || !srvDiffuse || !srvSpecular || !srvShadow || !rtvCombine)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvCombine, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { (_float)m_vBackgroundColor.r, (_float)m_vBackgroundColor.g, (_float)m_vBackgroundColor.b, 1.f };
	ctx->ClearRenderTargetView(rtvCombine, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	_float3 camPos = {};

	CMaterial* combineMat = Find_RectMaterial(CRenderTarget::RTType::Combine);

	combineMat->Bind_Matrix(w);
	combineMat->Bind_Camera(camPos, v, p, 0);

	ID3D11ShaderResourceView* srvs[5] = { srvAlbedo, srvDepth, srvDiffuse, srvSpecular, srvShadow };
	ctx->PSSetShaderResources(0, 5, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

const _int CCamera::GetColorPickingID(const vector2Int& _mouseVPPos)
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	auto& rtm = CRenderTargetManager::GetInstance();
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!device || !ctx)
		return 0;

	if (!EnsurePickStaging())
		return 0;

	ID3D11Texture2D* srcTex = rtm.GetTexture(CRenderTarget::RTType::Object, m_bIsEditor);
	if (!srcTex) return 0;

	D3D11_BOX box;
	box.left = _mouseVPPos.x;
	box.right = _mouseVPPos.x + 1;
	box.top = _mouseVPPos.y;
	box.bottom = _mouseVPPos.y + 1;
	box.front = 0;
	box.back = 1;

	ctx->CopySubresourceRegion
	(
		m_pPickStaging, 0,
		0, 0, 0,
		srcTex, 0,
		&box);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(ctx->Map(m_pPickStaging, 0, D3D11_MAP_READ, 0, &mapped)))
		return 0;

	const uint8_t* p = (const uint8_t*)mapped.pData;
	uint32_t r = p[0];
	uint32_t g = p[1];
	uint32_t b = p[2];

	ctx->Unmap(m_pPickStaging, 0);

	uint32_t id = (r) | (g << 8) | (b << 16);

	return id;
}

CPhysics::Ray CCamera::ScreenPointToRay(const vector2Int& _pixel, _float _maxDist)
{
	return CPhysics::Ray();
}

CPhysics::Ray CCamera::ScreenPointToRay_Editor(const vector2Int& _pixel, _float _maxDist)
{
	auto eo = CEditor::GetInstance().Get_Options();

	auto res = CEditor::GetInstance().Get_ScreenResolution();
	_float w = static_cast<_float>(res.x - 40);
	_float h = static_cast<_float>(res.y + 20);

	_float xNdc = 2.0f * _pixel.x / w - 1.0f;
	_float yNdc = -2.0f * _pixel.y / h + 1.0f;

	_vector ptNear = XMVectorSet(xNdc, yNdc, 0.f, 1.f);
	_vector ptFar = XMVectorSet(xNdc, yNdc, 1.f, 1.f);

	_matrix view = XMLoadFloat4x4(&m_vViewMatrix);
	_matrix proj = XMLoadFloat4x4(&m_vProjMatrix);
	_matrix invVP = XMMatrixInverse(nullptr, view * proj);

	ptNear = XMVector4Transform(ptNear, invVP);
	ptFar = XMVector4Transform(ptFar, invVP);
	ptNear /= XMVectorGetW(ptNear);
	ptFar /= XMVectorGetW(ptFar);

	_float3 origin, dir;
	XMStoreFloat3(&origin, ptNear);

	if (m_eCamViewMode == PERSPECTIVE)
	{
		_vector camPos = Get_Transform()->Get_WorldMatrix().r[3];
		_vector rayDir = XMVectorSubtract(ptFar, camPos);

		XMStoreFloat3(&origin, camPos);
		XMStoreFloat3(&dir, rayDir);
	}
	else
	{
		XMStoreFloat3(&origin, ptNear);
		dir = Get_Transform()->Get_Directions().forward;
	}

	vector3 resultDir = vector3(dir);

	CPhysics::Ray result = { origin, resultDir.normalized(), _maxDist };
	return result;
}

const _bool CCamera::EnsurePickStaging()
{
	if (m_pPickStaging)
		return true;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return false;

	D3D11_TEXTURE2D_DESC d = {};
	d.Width = 1;
	d.Height = 1;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_R32_UINT;
	d.SampleDesc.Count = 1;
	d.SampleDesc.Quality = 0;
	d.Usage = D3D11_USAGE_STAGING;
	d.BindFlags = 0;
	d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d.MiscFlags = 0;

	return SUCCEEDED(device->CreateTexture2D(&d, nullptr, &m_pPickStaging));
}

CMaterial* CCamera::Add_RectMaterial(const CRenderTarget::RTType _type, const wstring& _path)
{
	CMaterial* newMat = CResources::GetInstance().LoadOnGame<CMaterial>(_path);

	if (!newMat)
	{
		CDebug::LogError((wstring(L"Not found ") + _path));
		Safe_Release(m_pMainLight);
		return nullptr;
	}

	auto it = m_mRectMats.find(_type);

	if (it != m_mRectMats.end())
		Safe_Release(it->second);

	m_mRectMats[_type] = newMat;

	return newMat;
}

CMaterial* CCamera::Find_RectMaterial(const CRenderTarget::RTType _type)
{
	auto it = m_mRectMats.find(_type);

	if (it == m_mRectMats.end())
		return nullptr;

	return it->second;
}

const D3D11_VIEWPORT* CCamera::ResolveViewport() const
{
	return m_bIsEditor ? CGraphicDevice::GetInstance().Get_EditorViewport() : CGraphicDevice::GetInstance().Get_GameViewport();
}

void CCamera::Find_MainLight()
{
	auto& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightList();

	if (lights.empty())
	{
		if (m_pMainLight)
		{
			Safe_Release(m_pMainLight);
			m_pMainLight = nullptr;
		}
		return;
	}

	if (!m_pMainLight)
	{
		for (TRAVERSAL_ITER(lights, it))
		{
			if ((*it)->Get_Enable() && (*it)->Get_GameObject()->IsActive() && (*it)->IsCastShadow() && (*it)->Get_Type() == CLight::Type::Directional)
			{
				m_pMainLight = *it;
				m_pMainLight->AddRef();
				break;
			}
		}
	}
	else
	{
		if (!m_pMainLight->Get_GameObject()->IsActive() || !m_pMainLight->Get_Enable())
		{
			Safe_Release(m_pMainLight);
			m_pMainLight = nullptr;
		}
	}
}
