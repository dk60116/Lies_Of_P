// Camera.cpp

#include "epch.h"
#include "Camera.h"

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
	, m_vMeshList({})
	, m_vUIList({})
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

	if (dynamic_cast<CEditorCamera*>(this))
		return S_OK;

	// Rect
	m_pRectBuffer = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Rect (Mesh Buffer)");
	if (!m_pRectBuffer)
	{
		CDebug::LogError("Not found Rect (Mesh Buffer)");
		return E_FAIL;
	}
	m_pRectBuffer->AddRef();

	// Present Material (DeferredPresent.hlsl을 사용하는 머티리얼)
	CMaterial* presentMat = Add_RectMaterial(CRenderTarget::RTType::Defalut, L"DeferredPresent (Material)");
	CMaterial* depthPresentMat = Add_RectMaterial(CRenderTarget::RTType::Depth, L"DepthPresent (Material)");
	CMaterial* shadowDepthPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepthPresent, L"ShadowDepthPresent (Material)");
	CMaterial* shadowMaskPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMaskPresent, L"ShadowMaskPresent (Material)");
	CMaterial* combineMat = Add_RectMaterial(CRenderTarget::RTType::Combine, L"DeferredCombine (Material)");
	CMaterial* shadowDepthMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepth, L"ShadowDepth (Material)");
	CMaterial* shadingtMat = Add_RectMaterial(CRenderTarget::RTType::Shading, L"DeferredShading (Material)");
	CMaterial* specularMat = Add_RectMaterial(CRenderTarget::RTType::Specular, L"DeferredSpecular (Material)");
	CMaterial* shadowMaskMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMask, L"ShadowMask (Material)");

	// 5개 디스플레이 등록
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
	pushDisplay(CRenderTarget::RTType::Normal, presentMat);
	pushDisplay(CRenderTarget::RTType::Material, presentMat);
	pushDisplay(CRenderTarget::RTType::Depth, depthPresentMat);
	pushDisplay(CRenderTarget::RTType::ShadowDepth, shadowDepthPresentMat);
	pushDisplay(CRenderTarget::RTType::Shading, presentMat);
	pushDisplay(CRenderTarget::RTType::Specular, presentMat);
	pushDisplay(CRenderTarget::RTType::ShadowMask, shadowMaskPresentMat);

	// Debug pipeline states 생성
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return E_FAIL;

	// DepthStencil: DepthTest OFF
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

	// Rasterizer: Cull OFF
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

	// Blend: AlphaBlend
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
	m_fAspect = static_cast<_float>(CDisplay::GetInstance().Get_ScreenResolution().x) / CDisplay::GetInstance().Get_ScreenResolution().y;

	if (!m_pMainLight)
		Find_MainLight();
	
	if (m_pMainLight)
		m_pMainLight->BuildDirectionalShadow(this, CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().directionalLightShadowDist, m_sMainLightMatrix);

	Bind_ViewMatrix();
	Bind_ProjectionMatrix();
}

void CCamera::Render()
{
}

void CCamera::OnPostRender()
{
	m_vMeshList.clear();
}

void CCamera::OnDestroy()
{
	if (dynamic_cast<CEditorCamera*>(this))
		return;

	Safe_Release(m_pRectBuffer);

	for (TRAVERSAL_ITER(m_mRectMats, it))
		Safe_Release((*it).second);

	m_mRectMats.clear();

	m_mRTDebugDisplays.clear();

	Safe_Release(m_pRTDebugDS);
	Safe_Release(m_pRTShadowDepthDS);
	Safe_Release(m_pRTDebugRS);
	Safe_Release(m_pRTShdowDepthRS);
	Safe_Release(m_pRTDebugBS);

	Safe_Release(m_pInvViewProjCB);
	Safe_Release(m_pShadowCB);
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
	m_vMeshList.push_back(_mesh);
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
	for (TRAVERSAL_ITER(m_vMeshList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->Render_WithCamera(this);
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
	 if (dynamic_cast<CEditorCamera*>(this))
        return;

    if (!m_pRectBuffer)
        return;

    ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
    if (!ctx)
        return;

    auto& RTM = CRenderTargetManager::GetInstance();

    ID3D11ShaderResourceView* srvCombine = RTM.GetSRV(CRenderTarget::RTType::Combine);
    if (!srvCombine)
        return;

    // Combine을 Present할 머티리얼 (현재는 map에 Combine이 presentMat으로 등록되어 있음)
    CMaterial* presentMat = nullptr;
    {
        auto it = m_mRTDebugDisplays.find(CRenderTarget::RTType::Combine);
        if (it != m_mRTDebugDisplays.end())
            presentMat = it->second.material;
    }
    if (!presentMat)
        return;

    // --- 상태 백업
    ID3D11DepthStencilState* prevDS = nullptr; _uint prevStencilRef = 0;
    ID3D11RasterizerState* prevRS = nullptr;
    ID3D11BlendState* prevBS = nullptr; _float prevBlendFactor[4] = {}; _uint prevSampleMask = 0;

    ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
    ctx->RSGetState(&prevRS);
    ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

    D3D11_VIEWPORT prevVP{}; _uint prevVPCount = 1;
    ctx->RSGetViewports(&prevVPCount, &prevVP);

    // --- 뷰포트 (게임 뷰포트 우선)
    const D3D11_VIEWPORT* useVP = CGraphicDevice::GetInstance().Get_GameViewport();
    if (!useVP) useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();
    if (useVP) ctx->RSSetViewports(1, useVP);

    const float W = useVP ? useVP->Width  : (float)CDisplay::GetInstance().Get_ScreenResolution().x;
    const float H = useVP ? useVP->Height : (float)CDisplay::GetInstance().Get_ScreenResolution().y;

    // --- Present는 Depth 불필요(OFF), Cull OFF
    if (m_pRTDebugDS) ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
    if (m_pRTDebugRS) ctx->RSSetState(m_pRTDebugRS);
    const _float bf[4] = { 0,0,0,0 };
    ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

    // --- 픽셀 Ortho(좌상단 원점)
    _matrix v = XMMatrixIdentity();
    _matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
    _matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
    _float3 camPos = {}; // Present에는 의미 없음

    // --- SRV 충돌 방지 + 바인딩
    RTM.Unbind_AllSRVs_PS(ctx);

    presentMat->Bind_Matrix(w);
    presentMat->Bind_Camera(camPos, v, p, 0);

    ctx->PSSetShaderResources(0, 1, &srvCombine);
    m_pRectBuffer->Render();

    // --- 정리
    RTM.Unbind_AllSRVs_PS(ctx);

    // --- 상태 복원
    if (prevVPCount > 0) ctx->RSSetViewports(1, &prevVP);
    ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
    ctx->RSSetState(prevRS);
    ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

    Safe_Release(prevDS);
    Safe_Release(prevRS);
    Safe_Release(prevBS);
}

void CCamera::RenderRTDebugDisplay()
{
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	if (!context || m_mRTDebugDisplays.empty())
		return;

	// 상태 백업
	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	context->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	context->RSGetState(&prevRS);
	context->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	// 디버그 상태 적용
	context->OMSetDepthStencilState(m_pRTDebugDS, 0);
	context->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0,0,0,0 };
	context->OMSetBlendState(m_pRTDebugBS, bf, 0xFFFFFFFF);

	// 스크린 해상도
	auto res = CDisplay::GetInstance().Get_ScreenResolution();
	_float screenW = (_float)res.x;
	_float screenH = (_float)res.y;

	// 뷰포트(디버그 출력은 백버퍼 전체)
	{
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.f; vp.TopLeftY = 0.f;
		vp.Width = screenW; vp.Height = screenH;
		vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
		context->RSSetViewports(1, &vp);
	}

	// 픽셀 좌표 Ortho (좌상단 원점)
	_matrix view = XMMatrixIdentity();
	_matrix proj = XMMatrixOrthographicOffCenterLH(0.f, screenW, screenH, 0.f, 0.f, 1.f);
	_float3 camPos = { 0.f, 0.f, -1.f };

	const float margin = 12.f;
	const float gap = 10.f;

	// 게임 뷰포트 기준 aspect (없으면 화면 기준)
	const D3D11_VIEWPORT* gameVP = CGraphicDevice::GetInstance().Get_GameViewport();
	float srcW = gameVP ? gameVP->Width : screenW;
	float srcH = gameVP ? gameVP->Height : screenH;
	float srcAspect = (srcH > 0.f) ? (srcW / srcH) : 1.f;

	CRenderTarget::RTType types[] =
	{
		CRenderTarget::RTType::Albedo,
		CRenderTarget::RTType::Normal,
		CRenderTarget::RTType::Material,
		CRenderTarget::RTType::Depth,
		CRenderTarget::RTType::ShadowDepth,
		CRenderTarget::RTType::Shading,
		CRenderTarget::RTType::Specular,
		CRenderTarget::RTType::ShadowMask,
	};
	const _int kCount = (_int)(sizeof(types) / sizeof(types[0]));

	// 썸네일 크기(원하시면 여기 수치만 더 키우면 됩니다)
	const _uint screenes = CDisplay::GetInstance().Get_ScreenResolution().y;
	const _float resHeightFive = (CDisplay::GetInstance().Get_ScreenResolution().y / 5.f) * 1.6f;
	_float maxBox = resHeightFive;

	float rectW = maxBox;
	float rectH = rectW / srcAspect;
	if (rectH > maxBox)
	{
		rectH = maxBox;
		rectW = rectH * srcAspect;
	}

	for (int i = 0; i < kCount; ++i)
	{
		auto it = m_mRTDebugDisplays.find(types[i]);
		if (it == m_mRTDebugDisplays.end())
			continue;

		RTDebugDisplay& disp = it->second;
		if (!disp.quad || !disp.material)
			continue;

		const bool bRightColumn = (i < 5);
		const int row = bRightColumn ? i : (i - 5);

		float cx = bRightColumn
			? (screenW - margin - rectW * 0.5f)
			: (margin + rectW * 0.5f);

		float cy = screenH - margin - rectH * 0.5f - row * (rectH + gap);

		_matrix world = XMMatrixScaling(rectW, rectH, 1.f) * XMMatrixTranslation(cx, cy, 0.f);

		disp.material->Bind_Matrix(world);
		disp.material->Bind_Camera(camPos, view, proj, 0);

		ID3D11ShaderResourceView* srv = CRenderTargetManager::GetInstance().GetSRV(types[i]);
		context->PSSetShaderResources(0, 1, &srv);

		disp.quad->Render();
	}

	// SRV 해제
	CRenderTargetManager::GetInstance().Unbind_AllSRVs_PS(context);

	// 상태 복원
	context->OMSetDepthStencilState(prevDS, prevStencilRef);
	context->RSSetState(prevRS);
	context->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderLightingPass_ToShading(const D3D11_VIEWPORT* vp)
{
	CMaterial* shadingMat = Find_RectMaterial(CRenderTarget::RTType::Shading);

	if (!shadingMat || !m_pRectBuffer || !m_pInvViewProjCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx) 
		return;

	auto& RTM = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvNormal = RTM.GetSRV(CRenderTarget::RTType::Normal);
	ID3D11ShaderResourceView* srvDepth = RTM.GetSRV(CRenderTarget::RTType::Depth);
	ID3D11ShaderResourceView* srvMaterial = RTM.GetSRV(CRenderTarget::RTType::Material);

	ID3D11RenderTargetView* rtvShading = RTM.GetRTV(CRenderTarget::RTType::Shading);

	if (!srvNormal || !srvDepth || !srvMaterial || !rtvShading)
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

	RTM.Unbind_AllSRVs_PS(ctx);

	ctx->OMSetRenderTargets(1, &rtvShading, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : CGraphicDevice::GetInstance().Get_GameViewport();
	if (useVP) ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvShading, clear);

	if (m_pRTDebugDS) 
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS) 
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

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
	
	if (!lights.empty())
		shadingMat->Bind_Light(lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[3] = { srvNormal, srvDepth, srvMaterial };
	ctx->PSSetShaderResources(0, 3, srvs);

	m_pRectBuffer->Render();

	RTM.Unbind_AllSRVs_PS(ctx);

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

	auto& RTM = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvNormal = RTM.GetSRV(CRenderTarget::RTType::Normal);
	ID3D11ShaderResourceView* srvDepth = RTM.GetSRV(CRenderTarget::RTType::Depth);
	ID3D11ShaderResourceView* srvMaterial = RTM.GetSRV(CRenderTarget::RTType::Material);

	ID3D11RenderTargetView* rtvSpecular = RTM.GetRTV(CRenderTarget::RTType::Specular);

	if (!srvNormal || !srvDepth || !srvMaterial || !rtvSpecular)
		return;

	// --- 상태 백업
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

	// --- SRV 충돌 방지
	RTM.Unbind_AllSRVs_PS(ctx);

	// --- Specular RTV 바인딩 (Depth는 필요 없으면 nullptr로)
	ctx->OMSetRenderTargets(1, &rtvSpecular, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : CGraphicDevice::GetInstance().Get_GameViewport();
	if (useVP) ctx->RSSetViewports(1, useVP);

	// --- Clear (Specular은 기본 검정)
	const float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvSpecular, clear);

	// --- 디버그용 상태 재사용(DepthTest OFF / Cull OFF)
	if (m_pRTDebugDS) 
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS) 
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF); // 한 번에 모든 라이트 합산이면 블렌드 불필요

	// --- 풀스크린 쿼드용 카메라(픽셀 Ortho)
	_float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	// --- 실제 카메라 정보 (Specular PS에서 camPos 필요)
	_float3 camPos = Get_Transform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	// --- 머티리얼 바인딩
	specularMat->Bind_Matrix(w);
	specularMat->Bind_Camera(camPos, v, p, 0);

	// --- 라이트 바인딩
	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();

	if (!lights.empty())
		specularMat->Bind_Light(lights.data(), (_uint)lights.size());

	// --- SRV 바인딩: t0=null, t1=Normal, t2=Depth, t3=Material(=gSpecParams)
	ID3D11ShaderResourceView* srvs[3] = { srvNormal, srvDepth, srvMaterial };
	ctx->PSSetShaderResources(0, 3, srvs);

	// --- Draw
	m_pRectBuffer->Render();

	// --- 정리
	RTM.Unbind_AllSRVs_PS(ctx);

	// --- 상태 복원
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

void CCamera::RenderShadowDepthPass(const D3D11_VIEWPORT* vp)
{
	if (dynamic_cast<CEditorCamera*>(this))
		return;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!device || !ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11DepthStencilView* dsvShadow = rtm.GetDSV(CRenderTarget::RTType::ShadowDepth);
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

	rtm.Unbind_AllSRVs_PS(ctx);
	ctx->OMSetRenderTargets(0, nullptr, dsvShadow);

	const _uint shadowSize = (_uint)CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;
	D3D11_VIEWPORT vpt = {};
	vpt.TopLeftX = 0.f;
	vpt.TopLeftY = 0.f;
	vpt.Width = (float)shadowSize;
	vpt.Height = (float)shadowSize;
	vpt.MinDepth = 0.f;
	vpt.MaxDepth = 1.f;
	ctx->RSSetViewports(1, &vpt);

	// Clear (깊이 = 1)
	ctx->ClearDepthStencilView(dsvShadow, D3D11_CLEAR_DEPTH, 1.0f, 0);

	if (m_pRTShadowDepthDS)
		ctx->OMSetDepthStencilState(m_pRTShadowDepthDS, 0);
	if (m_pRTShdowDepthRS)
		ctx->RSSetState(m_pRTShdowDepthRS);

	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	auto& shadowList = m_vMeshList;

	CMaterial* shadowDepthMat = Find_RectMaterial(CRenderTarget::RTType::ShadowDepth);

	for (auto* r : shadowList)
	{
		if (!r) 
			continue;
		if (!r->Get_GameObject()->IsRecursiveActive()) 
			continue;
		if (!r->Get_Enable())
			continue;

		r->Render_ShadowDepth(shadowDepthMat, m_sMainLightMatrix);
	}

	rtm.Unbind_AllSRVs_PS(ctx);

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
	if (dynamic_cast<CEditorCamera*>(this))
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!ctx || !m_pRectBuffer || !m_pInvViewProjCB || !m_pShadowCB)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11RenderTargetView* rtvShadowMask = rtm.GetRTV(CRenderTarget::RTType::ShadowMask);
	ID3D11ShaderResourceView* srvSceneDepth = rtm.GetSRV(CRenderTarget::RTType::Depth);
	ID3D11ShaderResourceView* srvShadowDepth = rtm.GetSRV(CRenderTarget::RTType::ShadowDepth);

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

	rtm.Unbind_AllSRVs_PS(ctx);
	ctx->OMSetRenderTargets(1, &rtvShadowMask, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : CGraphicDevice::GetInstance().Get_GameViewport();

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

	_float W = useVP ? useVP->Width : (float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (float)CDisplay::GetInstance().Get_ScreenResolution().y;

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

	scb.bias = -0.0025f;

	ctx->UpdateSubresource(m_pShadowCB, 0, nullptr, &scb, 0, 0);
	ctx->PSSetConstantBuffers(6, 1, &m_pShadowCB);

	shadowMaskMat->Bind_Matrix(w);
	shadowMaskMat->Bind_Camera(camPos, v, p, 0);

	ID3D11ShaderResourceView* srvs[2] = { srvSceneDepth, srvShadowDepth };
	ctx->PSSetShaderResources(0, 2, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx);

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

void CCamera::RenderCombine(const D3D11_VIEWPORT* vp)
{
	if (!m_pRectBuffer)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& RTM = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = RTM.GetSRV(CRenderTarget::RTType::Albedo);
	ID3D11ShaderResourceView* srvShading = RTM.GetSRV(CRenderTarget::RTType::Shading);
	ID3D11ShaderResourceView* srvSpecular = RTM.GetSRV(CRenderTarget::RTType::Specular);
	ID3D11ShaderResourceView* srvShadow = RTM.GetSRV(CRenderTarget::RTType::ShadowMask);

	ID3D11RenderTargetView* rtvCombine = RTM.GetRTV(CRenderTarget::RTType::Combine);

	if (!srvAlbedo || !srvShading || !srvSpecular || !srvShadow || !rtvCombine)
		return;

	// --- 상태 백업
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

	// --- SRV 충돌 방지
	RTM.Unbind_AllSRVs_PS(ctx);

	// --- Specular RTV 바인딩 (Depth는 필요 없으면 nullptr로)
	ctx->OMSetRenderTargets(1, &rtvCombine, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : CGraphicDevice::GetInstance().Get_GameViewport();
	if (useVP) ctx->RSSetViewports(1, useVP);

	// --- Clear
	const _float clear[4] = { (_float)m_vBackgroundColor.r, (_float)m_vBackgroundColor.g, (_float)m_vBackgroundColor.b, 1.f };
	ctx->ClearRenderTargetView(rtvCombine, clear);

	// --- 디버그용 상태 재사용(DepthTest OFF / Cull OFF)
	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF); // 한 번에 모든 라이트 합산이면 블렌드 불필요

	// --- 풀스크린 쿼드용 카메라(픽셀 Ortho)
	_float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	// --- 더미 카메라 정보 
	_float3 camPos = {};

	InvViewProjCB invCB = { };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	CMaterial* combineMat = Find_RectMaterial(CRenderTarget::RTType::Combine);

	// --- 머티리얼 바인딩
	combineMat->Bind_Matrix(w);
	combineMat->Bind_Camera(camPos, v, p, 0);

	// --- SRV 바인딩
	ID3D11ShaderResourceView* srvs[4] = { srvAlbedo, srvShading, srvSpecular, srvShadow };
	ctx->PSSetShaderResources(0, 4, srvs);

	// --- Draw
	m_pRectBuffer->Render();

	// --- 정리
	RTM.Unbind_AllSRVs_PS(ctx);

	// --- 상태 복원
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

CMaterial* CCamera::Add_RectMaterial(const CRenderTarget::RTType _type, const wstring& _path)
{
	CMaterial* newMat = CResources::GetInstance().LoadOnGame<CMaterial>(_path);

	if (!newMat)
	{
		CDebug::LogError((wstring(L"Not found ") + _path));
		return nullptr;
	}

	auto it = m_mRectMats.find(_type);

	if (it != m_mRectMats.end()) 
		Safe_Release(it->second);

	m_mRectMats[_type] = newMat;
	newMat->AddRef();

	return newMat;
}

CMaterial* CCamera::Find_RectMaterial(const CRenderTarget::RTType _type)
{
	auto it = m_mRectMats.find(_type);

	if (it == m_mRectMats.end())
		return nullptr;

	return it->second;
}

void CCamera::Find_MainLight()
{
	auto& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightList();

	if (lights.empty())
		return;

	for (TRAVERSAL_ITER(lights, it))
	{
		if ((*it)->Get_Enable() && (*it)->Get_GameObject()->IsActive() && (*it)->IsCastShadow() && (*it)->Get_Type() == CLight::Type::Directional)
		{
			m_pMainLight = *it;
			break;
		}
	}
}
