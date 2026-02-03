#include "epch.h"
#include "Scene.h"
#include "EditorCamera.h"

CScene::CScene()
	: m_iSceneIndex(0)
	, m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_strSceneName(L"")
	, m_mResourceList({})
	, m_mTempResourceList({})
	, m_vCloneResourceList({})
	, m_mMeshBundleList({})
	, m_mTempMeshBundleList({})
	, m_mSkinnedBundleList({})
	, m_mSkinnedBoneList({})
	, m_mTempSkinnedBoneList({})
	, m_lObjectList({})
	, m_lCameraList({})
	, m_lLightList({})
	, m_lCanvasList({})
	, m_vLightData({})
	, m_pSkyBox(nullptr)
	, m_pEditorCamera(nullptr)
	, m_iUniqueObjectCount(0)
	, m_mObjectOfId({})
	, m_pSkyBoxDepthStencillState(nullptr)
	, m_pMeshDepthStencilState(nullptr)
	, m_pUIDepthStencilState(nullptr)
	, m_pSkyBoxResterizerState(nullptr)
	, m_pMeshResterizerState(nullptr)
	, m_pUIResterizerState(nullptr)
	, m_pBlendingState(nullptr)
	, m_pNoneBlendingState(nullptr)
	, m_fPssedTime(0.f)
{
	m_strName = L"Scene";

	m_pDevice = CGraphicDevice::GetInstance().Get_Device();
	m_pContext = CGraphicDevice::GetInstance().Get_Context();

	m_pDevice->AddRef();
	m_pContext->AddRef();
}

CScene::~CScene()
{
	SceneRelease();
}

HRESULT CScene::Initialize()
{
	SceneRelease();

	m_fPssedTime = 0.f;

	m_iUniqueObjectCount = 0;

	if (m_sEnviromentSettings.skyBox != L"")
	{
		if (!m_pSkyBox)
		{
			m_pSkyBox = CResources::GetInstance().LoadOnGame<CSkyBox>(m_sEnviromentSettings.skyBox);

			if (!m_pSkyBox)
				m_pSkyBox = CResources::GetInstance().LoadOnScene<CSkyBox>(m_sEnviromentSettings.skyBox);

			if (m_pSkyBox)
			{
				m_pSkyBox->AddRef();

				m_pSkyBox->Initialize_Scene();
			}
		}
	}

	m_mResourceList = m_mTempResourceList;
	m_mMeshBundleList = m_mTempMeshBundleList;
	m_mSkinnedBundleList = m_mTempSkinnedBundleList;
	m_mSkinnedBoneList = m_mTempSkinnedBoneList;

	for (TRAVERSAL_ITER(m_mTempMeshBundleList, it))
		(*it).second.clear();
	for (TRAVERSAL_ITER(m_mTempSkinnedBundleList, it))
		(*it).second.clear();
	for (TRAVERSAL_ITER(m_mTempSkinnedBoneList, it))
		(*it).second.clear();

	m_mTempResourceList.clear();
	m_mTempMeshBundleList.clear();
	m_mTempSkinnedBundleList.clear();
	m_mTempSkinnedBoneList.clear();

	// Sky Box
	{
		D3D11_RASTERIZER_DESC resterSkyDesc = {};
		resterSkyDesc.FillMode = D3D11_FILL_SOLID;
		resterSkyDesc.CullMode = D3D11_CULL_FRONT;
		resterSkyDesc.FrontCounterClockwise = FALSE;
		resterSkyDesc.DepthClipEnable = TRUE;

		D3D11_DEPTH_STENCIL_DESC depthSkyDesc = {};
		depthSkyDesc.DepthEnable = FALSE;
		depthSkyDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthSkyDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthSkyDesc.StencilEnable = FALSE;

		if (FAILED(m_pDevice->CreateRasterizerState(&resterSkyDesc, &m_pSkyBoxResterizerState)))
			return E_FAIL;
		if (FAILED(m_pDevice->CreateDepthStencilState(&depthSkyDesc, &m_pSkyBoxDepthStencillState)))
			return E_FAIL;
	}

	// Default
	{
		D3D11_RASTERIZER_DESC resterDefaultDesc = {};
		resterDefaultDesc.FillMode = D3D11_FILL_SOLID;
		resterDefaultDesc.CullMode = D3D11_CULL_BACK;
		resterDefaultDesc.FrontCounterClockwise = FALSE;
		resterDefaultDesc.DepthClipEnable = TRUE;

		D3D11_RASTERIZER_DESC resterNoneBlendDesc = {};
		resterNoneBlendDesc.FillMode = D3D11_FILL_SOLID;
		resterNoneBlendDesc.CullMode = D3D11_CULL_NONE;
		resterNoneBlendDesc.FrontCounterClockwise = FALSE;
		resterNoneBlendDesc.DepthClipEnable = TRUE;

		D3D11_DEPTH_STENCIL_DESC depthDefaultDesc = {};
		depthDefaultDesc.DepthEnable = TRUE;
		depthDefaultDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthDefaultDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthDefaultDesc.StencilEnable = FALSE;

		if (FAILED(m_pDevice->CreateRasterizerState(&resterDefaultDesc, &m_pMeshResterizerState)))
			return E_FAIL;

		if (FAILED(m_pDevice->CreateDepthStencilState(&depthDefaultDesc, &m_pMeshDepthStencilState)))
			return E_FAIL;
	}

	// Transparent
	{
		D3D11_RASTERIZER_DESC resterBlendDesc = {};
		resterBlendDesc.FillMode = D3D11_FILL_SOLID;
		resterBlendDesc.CullMode = D3D11_CULL_NONE;
		resterBlendDesc.FrontCounterClockwise = FALSE;
		resterBlendDesc.DepthClipEnable = TRUE;

		D3D11_DEPTH_STENCIL_DESC depthTransparentDesc = {};
		depthTransparentDesc.DepthEnable = TRUE;
		depthTransparentDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthTransparentDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthTransparentDesc.StencilEnable = FALSE;
	}

	// UI
	{
		D3D11_RASTERIZER_DESC resterUIDesc = {};
		resterUIDesc.FillMode = D3D11_FILL_SOLID;
		resterUIDesc.CullMode = D3D11_CULL_BACK;
		resterUIDesc.FrontCounterClockwise = FALSE;
		resterUIDesc.DepthClipEnable = FALSE;

		D3D11_DEPTH_STENCIL_DESC depthUIDesc = {};
		depthUIDesc.DepthEnable = FALSE;
		depthUIDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthUIDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depthUIDesc.StencilEnable = FALSE;

		if (FAILED(m_pDevice->CreateRasterizerState(&resterUIDesc, &m_pUIResterizerState)))
			return E_FAIL;
		if (FAILED(m_pDevice->CreateDepthStencilState(&depthUIDesc, &m_pUIDepthStencilState)))
			return E_FAIL;
	}

	// None Blending
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = FALSE;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		// NONE blending state
		if (FAILED(m_pDevice->CreateBlendState(&desc, &m_pNoneBlendingState)))
			return E_FAIL;
	}

	// Blending
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		// BLENDING state
		if (FAILED(m_pDevice->CreateBlendState(&desc, &m_pBlendingState)))
			return E_FAIL;
	}

	CDebug::Log(CDebug::MemoryUseLog());

#ifndef _CLIENT_BUILD
	CEditor::GetInstance().Set_SelectedGameObject(nullptr);
	CEditor::GetInstance().MoveTo_SelectedGameObject(nullptr);
	CGameObject* ecObj = Add_GameObject(L"Editor Camera Object");
	m_pEditorCamera = ecObj->AddComponent<CEditorCamera>();
	m_pEditorCamera->Get_Transform()->Set_Position(CEditor::GetInstance().Get_EditorCamPositon());
	m_pEditorCamera->Get_Transform()->Set_Quaternion(CEditor::GetInstance().Get_EditorCamQuaternion());
#endif

	CDebug::Log(L"Load scene Complete: " + m_strSceneName);

	return S_OK;
}

void CScene::Awake()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsRecursiveActive())
			(*it)->OnEnable();
	}

	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->Awake();

	Start();
}

void CScene::Start()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsRecursiveActive())
			(*it)->Start();
	}
}

void CScene::Update_Editor()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->Update_Editor();

	CPhysics::RAYCASTHIT firstHit = {};

	if (CInput::GetInstance().GetMouseButtonDown_Editor(0))
	{
		const vector2Int point = CInput::GetInstance().GetMousePos_Editor();
		CPhysics::Ray ray = m_pEditorCamera->ScreenPointToRay_Editor(point);

		auto hits = CPhysics::GetInstance().Raycast(ray);

		if (hits.size() <= 0)
			return;

		firstHit = hits[0];

		//CEditor::GetInstance().Set_SelectedGameObject(firstHit.object);

		if (CInput::GetInstance().GetKey_Editor(CONTROL))
		{
			CDebug::LogError("Ray Origin & Dir");
			CDebug::LogError(ray.origin);
			CDebug::LogError(ray.dir);
			CDebug::LogError("HitPos");
			CDebug::LogError(firstHit.hitPos);
			CDebug::LogError(firstHit.object->Get_ObjectName());

			CGameObject* newObj = Add_GameObject(L"AddObj");
			CMeshRenderer* newRen = newObj->AddComponent<CMeshRenderer>();
			newRen->Get_MeshFilter()->Set_MeshBuffer(CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Cube (Mesh Buffer)"));
			newRen->Get_Transform()->Get_Transform()->Set_Position(firstHit.hitPos);
		}
	}

	if (CInput::GetInstance().GetKey_Editor(CONTROL))
	{
		if (CInput::GetInstance().GetKeyDown_Editor(S))
		{
			wstring path = L"BinaryAssets/SceneData/" + m_strSceneName + L".scenedata";
			CResources::GetInstance().SaveSceneObjectTransformInfos(path, Convert_ObjectsTransformInfo());
		}
	}
}

void CScene::Update()
{
	if (CInput::GetInstance().GetMouseButtonDown(1))
	{
		const _uint id = Get_Camera()->GetColorPickingID(CInput::GetInstance().GetMousePos());
		CGameObject* pickedObj = FindGameObjectOfId(id);
		if (pickedObj)
			CDebug::LogError(L"Picked: " + pickedObj->Get_ObjectName());
	}

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsRecursiveActive())
			(*it)->Update();

		if ((*it)->m_bActive && !(*it)->m_bPrevActive)
		{
			(*it)->OnEnable();
			(*it)->Set_RecursiveActive(true);
		}

		if (!(*it)->m_bActive && (*it)->m_bPrevActive)
		{
			(*it)->OnDisable();
			(*it)->Set_RecursiveActive(false);
		}

		(*it)->m_bPrevActive = (*it)->m_bActive;
	}
}

void CScene::FixedUpdate()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsActive())
			(*it)->FixedUpdate();
	}
}

void CScene::LateUpdateEditor()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsActive())
			(*it)->LateUpdate_Editor();
	}
}

void CScene::LateUpdate()
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->IsActive())
			(*it)->LateUpdate();
	}
}

void CScene::Render_Editor()
{
#ifndef _CLIENT_BUILD
	const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_EditorContentViewport();
	if (!m_pEditorCamera)
		return;

	// 1) 라이트 데이터 갱신 (Render_Game과 동일)
	m_vLightData.clear();
	for (TRAVERSAL_ITER(m_lLightList, it))
	{
		if (!(*it)) continue;
		_float4x4 lightInfo = (*it)->To_LightInfo();
		lightInfo._44 = (_float)m_lLightList.size();
		m_vLightData.push_back(XMLoadFloat4x4(&lightInfo));
	}

	ID3D11DeviceContext* ctx = m_pContext;
	if (!ctx) return;

	// 2) 에디터 뷰포트
	const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_EditorViewport();
	if (!vp)
		vp = CGraphicDevice::GetInstance().Get_CurrentViewport();

	auto& trm = CRenderTargetManager::GetInstance();

	// ------------------------------------------------------------
	// A) Deferred: GBuffer 바인드/클리어
	// ------------------------------------------------------------
	trm.Bind_GBuffer(ctx, vp, /*isEditor=*/true);   // ★ 이런 형태로 분기 필요
	trm.Clear_GBuffer(/*isEditor=*/true);

	// Skybox (원하면)
	if (m_pSkyBox)
	{
		ctx->RSSetState(m_pSkyBoxResterizerState);
		ctx->OMSetDepthStencilState(m_pSkyBoxDepthStencillState, 0);
		RenderSkyBox(m_pEditorCamera);
	}

	// Mesh 기본 상태
	ctx->RSSetState(m_pMeshResterizerState);
	ctx->OMSetDepthStencilState(m_pMeshDepthStencilState, 0);

	// ------------------------------------------------------------
	// B) 여기서 "오브젝트들이 렌더 제출"을 해줘야 함
	// ------------------------------------------------------------
	// 기존 Render_Editor()에서 즉시 그리던 걸,
	//   - 각 Renderer가 m_pEditorCamera->Add_RenderTarget_Mesh(this) 식으로 제출
	// 로 바꿔야 editorCam->RenderMesh()가 의미가 생김.

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if (!(*it)->IsRecursiveActive())
			continue;

		(*it)->OnPreCull_Editor();
		(*it)->OnPreRender_Editor();

		// ★ 중요: 여기서 즉시 Draw 하지 말고 "제출"만 하도록 구조를 맞추는 게 좋음
		(*it)->Render_Editor(); // 내부에서 Renderer들이 editorCam에 Submit하도록
	}

	// 실제 드로우는 카메라가 한다
	m_pEditorCamera->RenderMesh();

	// ------------------------------------------------------------
	// C) Shadow / ObjectID / Lighting / Combine
	// ------------------------------------------------------------
	m_pEditorCamera->RenderShadowDepthPass(vp);
	m_pEditorCamera->RenderObjectIDPass(vp);
	m_pEditorCamera->RenderLightingPass_ToDiffuse(vp);
	m_pEditorCamera->RenderLightingPass_ToSpecular(vp);
	m_pEditorCamera->RenderShadowMaskPass(vp);
	m_pEditorCamera->RenderCombine(vp);

	// ------------------------------------------------------------
	// D) BackBuffer로 복귀 후 Present + Gizmo/UI
	// ------------------------------------------------------------
	// ★ 에디터 창 타겟으로 복귀 (너는 GameWindow를 쓰고 있으니 EditorWindow도 있어야 함)
	CGraphicDevice::GetInstance().Set_RenderTarget(CEditor::GetInstance().Get_EditorWindow());

	ColorValue back = ColorValue::gray(0.3f);
	CGraphicDevice::GetInstance().Clear_BackBuffer_View(&back);
	CGraphicDevice::GetInstance().Clear_DepthStencil_View();

	// Combine → BackBuffer
	m_pEditorCamera->RenderDisplay();

	// Gizmo는 보통 여기서 (3D 위에)
	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->Render_Gizmo();

	// RT Debug thumbnail 같은 것도 필요하면
	m_pEditorCamera->RenderRTDebugDisplay();

	// PostRender
	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->OnPostRender_Editor();

#endif
}

void CScene::Render_Game()
{
	m_vLightData.clear();

	for (TRAVERSAL_ITER(m_lLightList, it))
	{
		if (!(*it))
			continue;
		_float4x4 lightInfo = (*it)->To_LightInfo();
		lightInfo._44 = (_float)m_lLightList.size();
		m_vLightData.push_back(XMLoadFloat4x4(&lightInfo));
	}

	CGraphicDevice::GetInstance().Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());

	ColorValue backgroudColor = ColorValue::black();
	if (Get_Camera())
		backgroudColor = Get_Camera()->Get_BackgroundColor();

	for (TRAVERSAL_ITER(m_lObjectList, it))
		if ((*it)->IsActive())
			(*it)->Render();

	for (TRAVERSAL_ITER(m_lCameraList, it)) 
		(*it)->OnPreCull();
	for (TRAVERSAL_ITER(m_lCameraList, it)) 
		(*it)->OnPreRender();

	ID3D11DeviceContext* ctx = m_pContext;
	const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_GameViewport();
	if (!vp) 
		vp = CGraphicDevice::GetInstance().Get_CurrentViewport();

	auto& trm = CRenderTargetManager::GetInstance();

	trm.Bind_GBuffer(ctx, vp);
	trm.Clear_GBuffer();

	if (m_pSkyBox && !m_lCameraList.empty())
	{
		m_pContext->RSSetState(m_pSkyBoxResterizerState);
		m_pContext->OMSetDepthStencilState(m_pSkyBoxDepthStencillState, 0);
		RenderSkyBox(m_lCameraList.back());
	}

	m_pContext->RSSetState(m_pMeshResterizerState);
	m_pContext->OMSetDepthStencilState(m_pMeshDepthStencilState, 0);

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderMesh();
	}

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderShadowDepthPass(vp);
	}

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
		{
			(*it)->RenderObjectIDPass(vp);
			(*it)->RenderLightingPass_ToDiffuse(vp);
			(*it)->RenderLightingPass_ToSpecular(vp);
			(*it)->RenderShadowMaskPass(vp);
			(*it)->RenderCombine(vp);
		}
	}

	// BackBuffer 복귀 + UI/디버그
	CGraphicDevice::GetInstance().Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());
	CGraphicDevice::GetInstance().Clear_BackBuffer_View(&backgroudColor);
	CGraphicDevice::GetInstance().Clear_DepthStencil_View();

	// Combine Present를 먼저 백버퍼에 출력
	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderDisplay();
	}

	// 그 다음 UI
	m_pContext->RSSetState(m_pUIResterizerState);
	m_pContext->OMSetDepthStencilState(m_pUIDepthStencilState, 0);

	for (TRAVERSAL_ITER(m_lCameraList, it))
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderUI();

	// 이후 디버그(썸네일)
	for (TRAVERSAL_ITER(m_lCameraList, it))
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderRTDebugDisplay();

	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->OnPostRender();
}

void CScene::SceneRelease()
{
	Safe_Release(m_pSkyBox);
	m_pSkyBox = nullptr;

	m_lCameraList.clear();
	m_lLightList.clear();
	m_lCanvasList.clear();

	for (TRAVERSAL_ITER(m_lObjectList, it))
		Safe_Release(*it);
	for (TRAVERSAL_ITER(m_mResourceList, it))
		Safe_Release((*it).second);
	for (TRAVERSAL_ITER(m_mMeshBundleList, it))
	{
		for (TRAVERSAL_ITER((*it).second, it1))
		{
			Safe_Release((*it1).meshBuffer);
			Safe_Release((*it1).material);
			Safe_Release((*it1).texture);
		}

		(*it).second.clear();
	}
	for (TRAVERSAL_ITER(m_mSkinnedBundleList, it))
	{
		for (TRAVERSAL_ITER((*it).second, it1))
		{
			Safe_Release((*it1).meshBuffer);
			Safe_Release((*it1).material);
			Safe_Release((*it1).texture);
		}

		(*it).second.clear();
	}
	for (TRAVERSAL_ITER(m_mSkinnedBoneList, it))
		(*it).second.clear();
	for (TRAVERSAL_ITER(m_vCloneResourceList, it))
		Safe_Release(*it);

	m_lObjectList.clear();
	m_mResourceList.clear();
	m_mMeshBundleList.clear();
	m_mSkinnedBundleList.clear();
	m_mSkinnedBoneList.clear();
	m_vCloneResourceList.clear();

	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);
}

void CScene::RenderSkyBox(CCamera* _camera)
{
	if (m_pSkyBox)
		m_pSkyBox->RenderSky(_camera);
}

void CScene::Set_SceneName(const wstring _name)
{
	m_strSceneName = _name;
}

const wstring& CScene::Get_SceneName() const
{
	return m_strSceneName;
}

vector<CScene::SCENETRANSFORMINFO> CScene::Convert_ObjectsTransformInfo() const
{
	vector<SCENETRANSFORMINFO> result = {};

	_uint i = 0;

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		SCENETRANSFORMINFO info = {};

		CTransform* tf = (*it)->Get_Transform();

		info.objID = (*it)->m_iUniqueID;
		info.objName = (*it)->m_strGameObjectName;
		vector3 pos = tf->Get_LocalPosition();
		info.localPos = pos;
		const quaternion quat = tf->Get_LocalQuaternion();
		info.localQuaternion = quat;
		info.localScale = tf->Get_LocalScale();

		CRectTransform* rect = (*it)->GetComponent<CRectTransform>();

		info.isRect = rect ? true : false;

		if (info.isRect)
		{
			SCENERECTINFO rectInfo = {};

			rectInfo.anchoredPos = rect->Get_AnchoredPosition();
			rectInfo.widthHeight = rect->Get_WidthHeight();
			rectInfo.pivot = rect->Get_Pivot();
			rectInfo.anchorMin = rect->Get_Anchors().min;
			rectInfo.anchorMax = rect->Get_Anchors().max;

			info.rectInfo = rectInfo;
		}

		if (i > 0 && !(*it)->m_bIsBoneTransform)
			result.push_back(info);

		++i;
	}

	return result;
}

void CScene::Bind_ObjectsTransform(const vector<SCENETRANSFORMINFO> _infoList)
{
	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		CGameObject* obj = *it;

		if (obj->m_iUniqueID == 0)
			continue;

		for (TRAVERSAL_ITER(_infoList, it1))
		{
			if ((*it1).objID == obj->m_iUniqueID)
			{
				CTransform* tf = obj->Get_Transform();

				if (!(*it1).isRect)
					tf->Set_LocalScale((*it1).localScale);
				else
				{
					CRectTransform* rect = obj->GetComponent<CRectTransform>();
					rect->Set_Pivot((*it1).rectInfo.pivot);
					rect->Set_AnchorsMin((*it1).rectInfo.anchorMin);
					rect->Set_AnchorsMax((*it1).rectInfo.anchorMax);
					rect->Set_WidthHeight((*it1).rectInfo.widthHeight);
				}

				tf->Set_LocalQuaternion((*it1).localQuaternion);
				tf->Set_LocalPosition((*it1).localPos);
			}
		}
	}
}

CEngineResource* CScene::Add_Resource(const wstring& _name, CEngineResource* _resource)
{
	if (!_resource)
		return nullptr;

	auto [it, inserted] = m_mResourceList.try_emplace(_name, _resource);

	if (inserted)
	{
		_resource->AddRef();
		return _resource;
	}
	else
	{
		Safe_Release(_resource);
		return it->second;
	}
}

CEngineResource* CScene::Find_Resource(const wstring& _name)
{
	auto iter = m_mResourceList.find(_name);

	if (iter != m_mResourceList.end())
		return iter->second;

	auto iter1 = m_mTempResourceList.find(_name);

	if (iter1 != m_mTempResourceList.end())
		return iter1->second;

	return nullptr;
}

vector<MeshBundle> CScene::Find_MeshInfoResource(const wstring& _name)
{
	auto iter = m_mMeshBundleList.find(_name);

	if (iter != m_mMeshBundleList.end())
		return iter->second;

	auto iter1 = m_mTempMeshBundleList.find(_name);

	if (iter1 != m_mTempMeshBundleList.end())
		return iter1->second;

	return {};
}

vector<SkinnedMeshBundle> CScene::Find_SkinnedMeshInfoResource(const wstring& _name)
{
	auto iter = m_mSkinnedBundleList.find(_name);

	if (iter != m_mSkinnedBundleList.end())
		return iter->second;

	auto iter1 = m_mTempSkinnedBundleList.find(_name);

	if (iter1 != m_mTempSkinnedBundleList.end())
		return iter1->second;

	return {};
}

vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> CScene::Find_SkinnedBonesResource(const wstring& _name)
{
	auto iter = m_mSkinnedBoneList.find(_name);

	if (iter != m_mSkinnedBoneList.end())
		return iter->second;

	auto iter1 = m_mTempSkinnedBoneList.find(_name);

	if (iter1 != m_mTempSkinnedBoneList.end())
		return iter1->second;

	return {};
}

CEngineResource* CScene::Add_TempResource(const wstring& _name, CEngineResource* _resource)
{
	if (!_resource)
		return nullptr;

	m_mTempResourceList.emplace(_name, _resource);
	_resource->AddRef();

	return _resource;
}

void CScene::Add_MeshBundle(const wstring& _name, vector<MeshBundle> _resource)
{
	for (TRAVERSAL_ITER(_resource, it))
	{
		if ((*it).meshBuffer)
			(*it).meshBuffer->AddRef();
		if ((*it).material)
			(*it).material->AddRef();
		if ((*it).texture)
			(*it).texture->AddRef();
	}

	m_mMeshBundleList.emplace(_name, _resource);
}

void CScene::Add_SkinnedBundle(const wstring& _name, vector<SkinnedMeshBundle> _resource)
{
	for (TRAVERSAL_ITER(_resource, it))
	{
		if ((*it).meshBuffer)
			(*it).meshBuffer->AddRef();
		if ((*it).material)
			(*it).material->AddRef();
		if ((*it).texture)
			(*it).texture->AddRef();
	}

	m_mSkinnedBundleList.emplace(_name, _resource);
}

void CScene::Add_TempMeshBundle(const wstring& _name, vector<MeshBundle> _resource)
{
	for (TRAVERSAL_ITER(_resource, it))
	{
		if ((*it).meshBuffer)
			(*it).meshBuffer->AddRef();
		if ((*it).material)
			(*it).material->AddRef();
		if ((*it).texture)
			(*it).texture->AddRef();
	}

	m_mTempMeshBundleList.emplace(_name, _resource);
}

void CScene::Add_TempSkinnedBundle(const wstring& _name, vector<SkinnedMeshBundle> _resource)
{
	for (TRAVERSAL_ITER(_resource, it))
	{
		if ((*it).meshBuffer)
			(*it).meshBuffer->AddRef();
		if ((*it).material)
			(*it).material->AddRef();
		if ((*it).texture)
			(*it).texture->AddRef();
	}

	m_mTempSkinnedBundleList.emplace(_name, _resource);
}

void CScene::Add_SkinnedMeshBone(const wstring& _name, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _resource)
{
	m_mSkinnedBoneList.emplace(_name, _resource);
}

void CScene::Add_TempSkinnedMeshBone(const wstring& _name, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _resource)
{
	m_mTempSkinnedBoneList.emplace(_name, _resource);
}

CEngineResource* CScene::Add_CloneResourece(CEngineResource* _resource)
{
	if (!_resource)
		return nullptr;

	m_vCloneResourceList.push_back(_resource);
	_resource->AddRef();

	return _resource;
}

CGameObject* CScene::Add_GameObject(wstring _name)
{
	CGameObject* newObj = new CGameObject(_name, m_pDevice, m_pContext);
	newObj->AddRef();

	newObj->m_iUniqueID = m_iUniqueObjectCount++;

	m_lObjectList.push_back(newObj);

	newObj->Set_ObjectName(_name);

	if (FAILED(m_lObjectList.back()->Initialize()))
	{
		--m_iUniqueObjectCount;
		Safe_Release(newObj);
		return nullptr;
	}

	m_mObjectOfId.insert({ newObj->m_iUniqueID, newObj });
	newObj->Set_Scene(this);

	return newObj;
}

list<CGameObject*>& CScene::Get_ObjectList()
{
	return m_lObjectList;
}

vector<CGameObject*> CScene::Get_RootObjects()
{
	vector<CGameObject*> result = {};

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if ((*it)->Get_Transform()->Is_Root())
		{
			if ((*it)->m_iUniqueID != 0)
				result.push_back(*it);
		}
	}

	return result;
}

vector<CRenderer*> CScene::Get_MeshObjects()
{
	vector<CRenderer*> result = {};

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if (CRenderer* ren = (*it)->GetComponent<CRenderer>())
		{
			if ((*it)->m_iUniqueID != 0)
				result.push_back(ren);
		}
	}

	return result;
}

const CScene::EnviromentSettings& CScene::Get_EnviromentSetting()
{
	return m_sEnviromentSettings;
}

void CScene::Set_DirectionalLightShadowDist(const _float _value)
{
	m_sEnviromentSettings.directionalLightShadowDist = _value;
}

void CScene::Set_ShadwoBias(const _float _value)
{
	m_sEnviromentSettings.shadowBias = _value;
}

CCamera* CScene::Get_Camera() const
{
	if (m_lCameraList.size() <= 0)
		return nullptr;

	return m_lCameraList.back();
}

CCamera* CScene::Get_Camera(const _int _index) const
{
	_int i = 0;

	if (m_lCameraList.size() <= 0)
		return nullptr;

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		++i;

		if (_index == i)
			return (*it);
	}

	return m_lCameraList.back();
}

CCamera* CScene::Get_EditorCamera() const
{
	return m_pEditorCamera;
}

const list<class CCamera*>& CScene::Get_CameraList()
{
	return m_lCameraList;
}

CCamera* CScene::Add_Camera(CCamera* _camera)
{
	if (!_camera)
		return nullptr;

	m_lCameraList.push_back(_camera);

	return m_lCameraList.back();
}

const list<CLight*>& CScene::Get_LightList()
{
	return m_lLightList;
}

CLight* CScene::Add_Light(CLight* _light)
{
	if (!_light)
		return nullptr;

	m_lLightList.push_back(_light);

	return m_lLightList.back();
}

vector<_matrix>& CScene::Get_LightData()
{
	return m_vLightData;
}

CCanvas* CScene::Get_Canvas(const _int _index) const
{
	_int i = 0;

	if (m_lCanvasList.size() <= 0)
		return nullptr;

	for (TRAVERSAL_ITER(m_lCanvasList, it))
	{
		++i;

		if (_index == i)
			return (*it);
	}

	return m_lCanvasList.back();
}

const list<CCanvas*>& CScene::Get_CanvasList()
{
	return m_lCanvasList;
}

CCanvas* CScene::Add_Canvas(CCanvas* _canvas)
{
	if (!_canvas)
		return nullptr;

	m_lCanvasList.push_back(_canvas);
	m_lCanvasList.back()->AddRef();

	return m_lCanvasList.back();
}

HRESULT CScene::SaveScene(const wstring& _filePath)
{
	return S_OK;
}

const _uint CScene::Get_UniqueObjectCount() const
{
	return m_iUniqueObjectCount;
}

CGameObject* CScene::FindGameObjectOfId(const _uint id)
{
	auto it = m_mObjectOfId.find(id);

	if (it == m_mObjectOfId.end())
		return nullptr;
	
	return (*it).second;
}

HRESULT CScene::PreLoadResources()
{
	string path = "../Assets/Scenes/" + CEngineString::WStringToString(m_strSceneName) + ".scene";
	ifstream file(path);
	if (!file)
	{
		CDebug::LogError("Can not Open file");
		return E_FAIL;
	}

	string line;
	vector<string> nameList;
	vector<string> fileList;
	vector<string> formatList;

	while (getline(file, line))
	{
		if (CEngineString::Contains(line, "//"))
			continue;

		if (CEngineString::Contains(line, ':'))
		{
			auto split = CEngineString::Split(line, " : ");

			string name = "";
			string filepath = "";
			string format = "";

			name = split[0];
			filepath = split[1];

			if (split.size() >= 3)
				format = split[2];

			if (!CResources::FileExists(filepath))
			{
				nameList.push_back(name);
				fileList.push_back(filepath);
				formatList.push_back(format);
				CDebug::Log("Add File: " + filepath + " (Name: " + name + ")");
			}
			else
				CDebug::LogWarnning("Failed Add File: " + filepath);
		}
		else
			CDebug::LogError("Invalid line format: " + line);
	}

	CSceneLoader::GetInstance().StartLoading(nameList, fileList, formatList);

	return S_OK;
}

ID3D11DepthStencilState* CScene::Get_MeshStencillState() const
{
	return m_pMeshDepthStencilState;
}

ID3D11DepthStencilState* CScene::Get_UIStencillState() const
{
	return m_pUIDepthStencilState;
}