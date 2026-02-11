#include "epch.h"
#include "Scene.h"
#include "EditorCamera.h"
#include "MeshFilter.h"
#include "Renderer.h"
#include "Material.h"
#include "Texture.h"
#include "Animator.h"
#include "AnimationClip.h"
#include "AnimatorController.h"
#include <unordered_map>

namespace
{
	void DrawSelectedMeshBoundingBox(CCamera* camera)
	{
		if (!camera)
			return;

		CMeshBuffer* lineMesh = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Line (Mesh Buffer)");
		CMaterial* lineMat = CResources::GetInstance().LoadOnGame<CMaterial>(L"DefaultLineMaterial (Material)");

		if (!lineMesh || !lineMat)
			return;

		CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject();
		if (!selected)
			return;

		CMeshBuffer* meshBuffer = nullptr;
		CSkinnedMeshRenderer* skinnedRenderer = nullptr;
		_matrix objectWorld = selected->Get_Transform()->Get_WorldMatrix();
		if (CMeshRenderer* meshRenderer = selected->GetComponent<CMeshRenderer>())
		{
			meshBuffer = meshRenderer->Get_MeshBuffer();
		}
		else if ((skinnedRenderer = selected->GetComponent<CSkinnedMeshRenderer>()))
		{
			meshBuffer = skinnedRenderer->Get_MeshBuffer();

			vector<CTransform*>& rootBones = skinnedRenderer->GetRootBons();
			if (!rootBones.empty() && rootBones[0])
				objectWorld = rootBones[0]->Get_WorldMatrix();
		}

		if (!meshBuffer)
			return;

		const CMeshBuffer::MESHBUFFERDESC& desc = meshBuffer->Get_Info();
		const BoundingBox& localBox = desc.boundingBox;

		_vector center = XMLoadFloat3(&localBox.Center);
		_vector extents = XMLoadFloat3(&localBox.Extents);

		_vector offsets[8] =
		{
			XMVectorSet(-1.f, -1.f, -1.f, 0.f),
			XMVectorSet( 1.f, -1.f, -1.f, 0.f),
			XMVectorSet( 1.f,  1.f, -1.f, 0.f),
			XMVectorSet(-1.f,  1.f, -1.f, 0.f),
			XMVectorSet(-1.f, -1.f,  1.f, 0.f),
			XMVectorSet( 1.f, -1.f,  1.f, 0.f),
			XMVectorSet( 1.f,  1.f,  1.f, 0.f),
			XMVectorSet(-1.f,  1.f,  1.f, 0.f)
		};

		_vector worldCorners[8] = {};
		if (skinnedRenderer)
		{
			_float3 minBound = {};
			_float3 maxBound = {};
			if (skinnedRenderer->TryGetAnimatedWorldBounds(minBound, maxBound))
			{
				worldCorners[0] = XMVectorSet(minBound.x, minBound.y, minBound.z, 1.f);
				worldCorners[1] = XMVectorSet(maxBound.x, minBound.y, minBound.z, 1.f);
				worldCorners[2] = XMVectorSet(maxBound.x, maxBound.y, minBound.z, 1.f);
				worldCorners[3] = XMVectorSet(minBound.x, maxBound.y, minBound.z, 1.f);
				worldCorners[4] = XMVectorSet(minBound.x, minBound.y, maxBound.z, 1.f);
				worldCorners[5] = XMVectorSet(maxBound.x, minBound.y, maxBound.z, 1.f);
				worldCorners[6] = XMVectorSet(maxBound.x, maxBound.y, maxBound.z, 1.f);
				worldCorners[7] = XMVectorSet(minBound.x, maxBound.y, maxBound.z, 1.f);
			}
			else
			{
				for (_uint i = 0; i < 8; ++i)
				{
					_vector localCorner = center + XMVectorMultiply(offsets[i], extents);
					worldCorners[i] = XMVector3Transform(localCorner, objectWorld);
				}
			}
		}
		else
		{
			for (_uint i = 0; i < 8; ++i)
			{
				_vector localCorner = center + XMVectorMultiply(offsets[i], extents);
				worldCorners[i] = XMVector3Transform(localCorner, objectWorld);
			}
		}

		constexpr _uint edges[12][2] =
		{
			{0,1}, {1,2}, {2,3}, {3,0},
			{4,5}, {5,6}, {6,7}, {7,4},
			{0,4}, {1,5}, {2,6}, {3,7}
		};

		_vector camPosV = camera->Get_Transform()->Get_Position().toXMVector();
		_float3 camPos = {};
		XMStoreFloat3(&camPos, camPosV);
		_matrix view = camera->Get_ViewMatrix();
		_matrix proj = camera->Get_ProjectionMatrix();

		for (const auto& edge : edges)
		{
			_vector a = worldCorners[edge[0]];
			_vector b = worldCorners[edge[1]];
			_vector delta = b - a;
			_float length = XMVectorGetX(XMVector3Length(delta));

			if (length <= 0.0001f)
				continue;

			_vector dir = XMVector3Normalize(delta);
			_vector xAxis = XMVectorSet(1.f, 0.f, 0.f, 0.f);
			_float dot = XMVectorGetX(XMVector3Dot(xAxis, dir));
			_matrix rot = XMMatrixIdentity();

			if (dot < 0.9999f)
			{
				if (dot > -0.9999f)
				{
					_vector axis = XMVector3Normalize(XMVector3Cross(xAxis, dir));
					_float angle = acosf(dot);
					rot = XMMatrixRotationAxis(axis, angle);
				}
				else
				{
					rot = XMMatrixRotationAxis(XMVectorSet(0.f, 1.f, 0.f, 0.f), XM_PI);
				}
			}

			_vector mid = (a + b) * 0.5f;
			_matrix scale = XMMatrixScaling(length, 1.f, 1.f);
			_matrix trans = XMMatrixTranslationFromVector(mid);
			_matrix world = scale * rot * trans;

			lineMat->Bind_Matrix(world);
			lineMat->Bind_Camera(camPos, view, proj, 0);
			lineMesh->Render();
		}
	}
}

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
	, m_vTempPickMousePos({-1, -1})
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
	PickObjectInEditor_End();
	PickObjectInEditor_Start();

	if (CInput::GetInstance().GetKeyDown_Editor(KEY_DELETE))
	{
		if (auto selected = CEditor::GetInstance().Get_SelectedGameObject())
		{
			selected->Destroy();
			CEditor::GetInstance().Set_SelectedGameObject(nullptr);
		}
	}

	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->Update_Editor();

	CPhysics::RAYCASTHIT firstHit = {};

	if (!ImGui::GetIO().WantCaptureMouse && CInput::GetInstance().GetMouseButtonDown_Editor(0))
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
		}
	}

	if (CInput::GetInstance().GetKey_Editor(CONTROL))
	{
		if (CInput::GetInstance().GetKeyDown_Editor(S))
		{
			wstring scenePath = L"../Assets/Scenes/" + m_strSceneName + L".scene";
			SaveScene(scenePath);
		}
	}
}

void CScene::Update()
{
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
	if (!m_pEditorCamera)
		return;

	m_vLightData.clear();
	for (TRAVERSAL_ITER(m_lLightList, it))
	{
		if (!(*it))
			continue;
		_float4x4 lightInfo = (*it)->To_LightInfo();
		lightInfo._44 = (_float)m_lLightList.size();
		m_vLightData.push_back(XMLoadFloat4x4(&lightInfo));
	}

	ID3D11DeviceContext* ctx = m_pContext;

	if (!ctx) 
		return;

	const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_EditorViewport();
	if (!vp)
		vp = CGraphicDevice::GetInstance().Get_CurrentViewport();

	D3D11_VIEWPORT editorRTVP = {};
	const D3D11_VIEWPORT* rtVP = vp;
	if (vp)
	{
		editorRTVP = *vp;
		editorRTVP.TopLeftX = 0.f;
		editorRTVP.TopLeftY = 0.f;
		rtVP = &editorRTVP;
	}

	auto& trm = CRenderTargetManager::GetInstance();

	trm.Bind_GBuffer(ctx, rtVP, true); 
	trm.Clear_GBuffer(true);

	if (m_pSkyBox)
	{
		ctx->RSSetState(m_pSkyBoxResterizerState);
		ctx->OMSetDepthStencilState(m_pSkyBoxDepthStencillState, 0);
		RenderSkyBox(m_pEditorCamera);
	}

	ctx->RSSetState(m_pMeshResterizerState);
	ctx->OMSetDepthStencilState(m_pMeshDepthStencilState, 0);

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if (!(*it)->IsRecursiveActive())
			continue;

		(*it)->OnPreCull_Editor();
		(*it)->OnPreRender_Editor();

		(*it)->Render_Editor(); 
	}

	DrawSelectedMeshBoundingBox(m_pEditorCamera);
	m_pEditorCamera->RenderMesh();

	m_pEditorCamera->RenderShadowDepthPass(rtVP);
	m_pEditorCamera->RenderObjectIDPass(rtVP);
	m_pEditorCamera->RenderLightingPass_ToDiffuse(rtVP);
	m_pEditorCamera->RenderLightingPass_ToSpecular(rtVP);
	m_pEditorCamera->RenderShadowMaskPass(rtVP);
	m_pEditorCamera->RenderCombine(rtVP);

	CGraphicDevice::GetInstance().Set_RenderTarget(CEditor::GetInstance().Get_EditorWindow());

	ColorValue back = ColorValue::gray(0.3f);
	CGraphicDevice::GetInstance().Clear_BackBuffer_View(&back);
	CGraphicDevice::GetInstance().Clear_DepthStencil_View();

	m_pEditorCamera->RenderDisplay();

	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->Render_Gizmo();

	m_pEditorCamera->RenderRTDebugDisplay(true);

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

	CGraphicDevice::GetInstance().Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());
	CGraphicDevice::GetInstance().Clear_BackBuffer_View(&backgroudColor);
	CGraphicDevice::GetInstance().Clear_DepthStencil_View();

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderDisplay();
	}

	m_pContext->RSSetState(m_pUIResterizerState);
	m_pContext->OMSetDepthStencilState(m_pUIDepthStencilState, 0);

	for (TRAVERSAL_ITER(m_lCameraList, it))
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderUI();

	for (TRAVERSAL_ITER(m_lCameraList, it))
		if ((*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
			(*it)->RenderRTDebugDisplay(false);

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

void CScene::EndFrame()
{
	for (auto it = m_lObjectList.begin(); it != m_lObjectList.end(); )
	{
		CGameObject* obj = *it;

		if (obj && obj->m_bKill)
		{
			Safe_Release(obj);
			it = m_lObjectList.erase(it); 
		}
		else
			++it;
	}
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

	auto buildPath = [](CGameObject* obj)
	{
		vector<wstring> names;
		CTransform* current = obj->Get_Transform();
		while (current)
		{
			CGameObject* currentObj = current->Get_GameObject();
			if (currentObj)
				names.push_back(currentObj->Get_ObjectName());
			current = current->Get_Parent();
		}

		wstring path = L"";
		for (auto it = names.rbegin(); it != names.rend(); ++it)
		{
			if (!path.empty())
				path += L"/";
			path += *it;
		}

		return path;
	};

	_uint i = 0;

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		SCENETRANSFORMINFO info = {};

		CTransform* tf = (*it)->Get_Transform();

		info.objID = (*it)->m_iUniqueID;
		info.objGuid = (*it)->Get_Guid();
		info.objName = (*it)->m_strGameObjectName;
		info.objPath = buildPath(*it);
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

		for (CComponent* component : (*it)->Get_ComponentList())
		{
			if (!component)
				continue;

			if (dynamic_cast<CTransform*>(component) || dynamic_cast<CRectTransform*>(component))
				continue;

			info.componentNames.push_back(component->Get_UName());
		}

		if (i > 0 && !(*it)->m_bIsBoneTransform)
			result.push_back(info);

		++i;
	}

	return result;
}

void CScene::Bind_ObjectsTransform(const vector<SCENETRANSFORMINFO> _infoList)
{
	unordered_map<wstring, size_t> infoIndexByGuid;
	unordered_map<wstring, size_t> infoIndexByPath;
	infoIndexByGuid.reserve(_infoList.size());
	infoIndexByPath.reserve(_infoList.size());
	unordered_map<wstring, vector<size_t>> infoIndicesByName;
	infoIndicesByName.reserve(_infoList.size());
	vector<_bool> usedInfo(_infoList.size(), false);

	for (size_t i = 0; i < _infoList.size(); ++i)
	{
		const auto& info = _infoList[i];
		if (!info.objGuid.empty())
			infoIndexByGuid.emplace(info.objGuid, i);
		if (!info.objPath.empty())
			infoIndexByPath.emplace(info.objPath, i);
		if (!info.objName.empty())
			infoIndicesByName[info.objName].push_back(i);
	}

	auto buildPath = [](CGameObject* obj)
	{
		vector<wstring> names;
		CTransform* current = obj->Get_Transform();
		while (current)
		{
			CGameObject* currentObj = current->Get_GameObject();
			if (currentObj)
				names.push_back(currentObj->Get_ObjectName());
			current = current->Get_Parent();
		}

		wstring path = L"";
		for (auto it = names.rbegin(); it != names.rend(); ++it)
		{
			if (!path.empty())
				path += L"/";
			path += *it;
		}

		return path;
	};

	auto applyInfo = [](CGameObject* obj, const SCENETRANSFORMINFO& info)
	{
		CTransform* tf = obj->Get_Transform();

		if (!info.isRect)
			tf->Set_LocalScale(info.localScale);
		else
		{
			CRectTransform* rect = obj->GetComponent<CRectTransform>();
			if (rect)
			{
				rect->Set_Pivot(info.rectInfo.pivot);
				rect->Set_AnchorsMin(info.rectInfo.anchorMin);
				rect->Set_AnchorsMax(info.rectInfo.anchorMax);
				rect->Set_WidthHeight(info.rectInfo.widthHeight);
			}
			else
			{
				tf->Set_LocalScale(info.localScale);
			}
		}

		tf->Set_LocalQuaternion(info.localQuaternion);
		tf->Set_LocalPosition(info.localPos);
	};

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		CGameObject* obj = *it;

		if (obj->m_iUniqueID == 0)
			continue;

		auto guidIter = infoIndexByGuid.find(obj->Get_Guid());
		if (guidIter != infoIndexByGuid.end() && !usedInfo[guidIter->second])
		{
			const auto& info = _infoList[guidIter->second];
			applyInfo(obj, info);
			usedInfo[guidIter->second] = true;
			continue;
		}

		const wstring path = buildPath(obj);
		auto pathIter = infoIndexByPath.find(path);
		if (pathIter != infoIndexByPath.end() && !usedInfo[pathIter->second])
		{
			const auto& info = _infoList[pathIter->second];
			if (!info.objGuid.empty())
				obj->m_strGuid = info.objGuid;
			applyInfo(obj, info);
			usedInfo[pathIter->second] = true;
			continue;
		}

		auto nameIter = infoIndicesByName.find(obj->Get_ObjectName());
		if (nameIter != infoIndicesByName.end())
		{
			auto& indices = nameIter->second;
			size_t matchedIndex = indices.size();
			for (size_t i = 0; i < indices.size(); ++i)
			{
				if (!usedInfo[indices[i]])
				{
					matchedIndex = indices[i];
					usedInfo[indices[i]] = true;
					break;
				}
			}

			if (matchedIndex < _infoList.size())
			{
				const auto& info = _infoList[matchedIndex];
				if (!info.objGuid.empty())
					obj->m_strGuid = info.objGuid;
				applyInfo(obj, info);
			}
		}
	}

	auto splitPath = [](const wstring& path)
	{
		vector<wstring> parts;
		size_t start = 0;
		while (start < path.size())
		{
			size_t slash = path.find(L'/', start);
			if (slash == wstring::npos)
				slash = path.size();
			if (slash > start)
				parts.push_back(path.substr(start, slash - start));
			start = slash + 1;
		}
		return parts;
	};

	auto findObjectByPath = [&](const wstring& path)
	{
		for (TRAVERSAL_ITER(m_lObjectList, itObj))
		{
			CGameObject* obj = *itObj;
			if (obj->m_iUniqueID == 0)
				continue;
			if (buildPath(obj) == path)
				return obj;
		}
		return static_cast<CGameObject*>(nullptr);
	};

	_bool createdAny = true;
	while (createdAny)
	{
		createdAny = false;
		for (size_t i = 0; i < _infoList.size(); ++i)
		{
			if (usedInfo[i])
				continue;

			const auto& info = _infoList[i];
			if (info.objPath.empty())
				continue;

			if (findObjectByPath(info.objPath))
			{
				usedInfo[i] = true;
				continue;
			}

			vector<wstring> parts = splitPath(info.objPath);
			if (parts.empty())
				continue;

			wstring parentPath;
			for (size_t partIdx = 0; partIdx + 1 < parts.size(); ++partIdx)
			{
				if (!parentPath.empty())
					parentPath += L"/";
				parentPath += parts[partIdx];
			}

			CGameObject* parentObj = nullptr;
			if (!parentPath.empty())
			{
				parentObj = findObjectByPath(parentPath);
				if (!parentObj)
					continue;
			}

			CGameObject* newObj = Add_GameObject(parts.back());
			if (!newObj)
				continue;

			if (parentObj)
				newObj->Get_Transform()->SetParent(parentObj->Get_Transform());

			if (!info.objGuid.empty())
				newObj->m_strGuid = info.objGuid;

			applyInfo(newObj, info);
			usedInfo[i] = true;
			createdAny = true;
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

	if (!dynamic_cast<CEditorCamera*>(_camera))
		m_lCameraList.push_back(_camera);
	else
		return nullptr;

	return m_lCameraList.back();
}

void CScene::Remove_Camera(CCamera* _camera)
{
	if (_camera)
		m_lCameraList.remove(_camera);
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
	wstring sceneDataPath = L"BinaryAssets/SceneData/" + m_strSceneName + L".scenedata";
	if (FAILED(CResources::GetInstance().SaveSceneObjectTransformInfos(sceneDataPath, Convert_ObjectsTransformInfo())))
		return E_FAIL;

	auto normalizePath = [](wstring path)
	{
		path = CEngineString::Replace(path, L"\\", L"/");
		const wstring prefix = L"../Assets/";
		if (path.rfind(prefix, 0) == 0)
			path = path.substr(prefix.size());
		return path;
	};

	struct SceneResourceEntry
	{
		wstring name;
		wstring path;
		wstring format;
	};

	unordered_map<wstring, SceneResourceEntry> previousEntries;
	{
		ifstream prev(_filePath);
		string line;
		while (getline(prev, line))
		{
			if (line.empty() || CEngineString::Contains(line, "//") || !CEngineString::Contains(line, " : "))
				continue;

			auto split = CEngineString::Split(line, " : ");
			if (split.size() < 3)
				continue;

			SceneResourceEntry e = {};
			e.name = CEngineString::StringToWString(split[0]);
			e.path = normalizePath(CEngineString::StringToWString(split[1]));
			e.format = CEngineString::StringToWString(split[2]);
			previousEntries[e.name] = e;
		}
	}

	unordered_map<wstring, SceneResourceEntry> entries;

	auto trimResourceSuffix = [](const wstring& resourceName)
	{
		static const vector<wstring> suffixes =
		{
			L" (Texture)",
			L" (MeshBuffer)",
			L" (Animation Clip)",
			L" (Animator Controller)",
			L" (Material)",
			L" (SkyBox)"
		};

		for (const auto& suffix : suffixes)
		{
			if (resourceName.size() >= suffix.size())
			{
				size_t pos = resourceName.size() - suffix.size();
				if (resourceName.compare(pos, suffix.size(), suffix) == 0)
					return resourceName.substr(0, pos);
			}
		}

		return resourceName;
	};

	auto addEntry = [&](const wstring& name, const wstring& path, const wstring& format)
	{
		if (name.empty())
			return;

		SceneResourceEntry entry = {};
		entry.name = name;
		entry.path = normalizePath(path);
		entry.format = format;

		if (entry.path.empty() || entry.format.empty())
		{
			auto it = previousEntries.find(name);
			if (it != previousEntries.end())
			{
				if (entry.path.empty())
					entry.path = it->second.path;
				if (entry.format.empty())
					entry.format = it->second.format;
			}
		}

		if (entry.path.empty() || entry.format.empty())
			return;

		entries[name] = entry;
	};

	auto addResourceWithName = [&](const wstring& sceneName, CEngineResource* resource)
	{
		if (!resource || sceneName.empty())
			return;

		wstring format = L"";
		if (dynamic_cast<CTexture*>(resource))
			format = L"[Texture]";
		else if (dynamic_cast<CSkinnedMeshBuffer*>(resource))
			format = L"[Skinned Mesh]";
		else if (dynamic_cast<CMeshBuffer*>(resource))
			format = L"[Mesh]";
		else if (auto clip = dynamic_cast<CAnimationClip*>(resource))
		{
			format = L"[Animation Clip]";
			if (clip->IsLoop())
				format += L" [Loop]";
		}
		else if (dynamic_cast<CAnimatorController*>(resource))
			format = L"[Animator Controller]";

		addEntry(sceneName, resource->Get_FilePath(), format);
	};

	auto addResource = [&](CEngineResource* resource)
	{
		if (!resource)
			return;

		const wstring sceneName = trimResourceSuffix(resource->Get_ResourceName());
		addResourceWithName(sceneName, resource);
	};

	for (CGameObject* obj : m_lObjectList)
	{
		if (!obj)
			continue;

		for (CComponent* component : obj->Get_ComponentList())
		{
			if (!component)
				continue;

			if (CMeshFilter* meshFilter = dynamic_cast<CMeshFilter*>(component))
				addResource(meshFilter->Get_MeshBuffer());

			if (CRenderer* renderer = dynamic_cast<CRenderer*>(component))
			{
				CMaterial* material = renderer->Get_Material();
				addResource(material);

				if (material)
				{
					for (_uint i = 0; i < material->Get_TextureCount(); ++i)
						addResource(material->Get_Texture(static_cast<_int>(i)));
				}
			}

			if (auto animator = dynamic_cast<CAnimator*>(component))
			{
				for (auto& [clipName, clip] : animator->Get_AnimationClipList())
					addResourceWithName(clipName, clip);
			}
		}
	}

	vector<SceneResourceEntry> sortedEntries;
	sortedEntries.reserve(entries.size());
	for (auto& [key, value] : entries)
		sortedEntries.push_back(value);

	sort(sortedEntries.begin(), sortedEntries.end(), [](const SceneResourceEntry& a, const SceneResourceEntry& b)
	{
		return a.name < b.name;
	});

	ofstream out(_filePath);
	if (!out.is_open())
	{
		CDebug::LogError(L"SaveScene failed - can not open: " + _filePath);
		return E_FAIL;
	}

	out << "SceneName : " << CEngineString::WStringToString(m_strSceneName) << "\n";
	for (const auto& entry : sortedEntries)
	{
		out
			<< CEngineString::WStringToString(entry.name)
			<< " : "
			<< CEngineString::WStringToString(entry.path)
			<< " : "
			<< CEngineString::WStringToString(entry.format)
			<< "\n";
	}

	out.close();

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

void CScene::PickObjectInEditor_Start()
{
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	if (CInput::GetInstance().GetMouseButtonDown_Editor(0))
		m_vTempPickMousePos = CInput::GetInstance().GetMousePos_Editor();
}

void CScene::PickObjectInEditor_End()
{
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	if (CInput::GetInstance().GetMouseButtonUp_Editor(0))
	{
		const vector2Int mp = CInput::GetInstance().GetMousePos_Editor();

		if (m_vTempPickMousePos == mp)
		{
			const _uint id = Get_EditorCamera()->GetColorPickingID(mp);
			CGameObject* pickedObj = FindGameObjectOfId(id);
			if (id != 0 && pickedObj)
				CEditor::GetInstance().Set_SelectedGameObject(pickedObj, true);
		}
	}
}

ID3D11DepthStencilState* CScene::Get_MeshStencillState() const
{
	return m_pMeshDepthStencilState;
}

ID3D11DepthStencilState* CScene::Get_UIStencillState() const
{
	return m_pUIDepthStencilState;
}
