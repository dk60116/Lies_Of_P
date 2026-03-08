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
#include "Camera.h"
#include "Light.h"
#include "MeshRenderer.h"
#include "SkinnedMeshRenderer.h"
#include "UI.h"
#include "Canvas.h"
#include "Terrain.h"
#include "Image.h"
#include "Text.h"
#include "CapsuleCollider.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "MeshCollider.h"
#include "RigidBody.h"
#include "Physics.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{
	_bool BuildLineWorldMatrix(const _vector& a, const _vector& b, _matrix& outWorld)
	{
		_vector delta = b - a;
		_float length = XMVectorGetX(XMVector3Length(delta));

		if (length <= 0.0001f)
			return false;

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
		outWorld = scale * rot * trans;
		return true;
	}

	_bool BuildLineResources(CCamera* camera, CMeshBuffer*& outLineMesh, CMaterial*& outLineMat, _float3& outCamPos, _matrix& outView, _matrix& outProj)
	{
		if (!camera)
			return false;

		outLineMesh = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Line (Mesh Buffer)");
		outLineMat = CResources::GetInstance().LoadOnGame<CMaterial>(L"DefaultLineMaterial (Material)");

		if (!outLineMesh || !outLineMat)
			return false;

		_vector camPosV = camera->Get_Transform()->Get_Position().toXMVector();
		XMStoreFloat3(&outCamPos, camPosV);
		outView = camera->GetViewMatrix();
		outProj = camera->GetProjectionMatrix();
		return true;
	}

	void DrawLineSegment(CMeshBuffer* lineMesh, CMaterial* lineMat, const _float3& camPos, const _matrix& view, const _matrix& proj, const _vector& a, const _vector& b)
	{
		_matrix world = XMMatrixIdentity();
		if (!BuildLineWorldMatrix(a, b, world))
			return;

		lineMat->Bind_Matrix(world);
		lineMat->Bind_Camera(camPos, view, proj, 0);
		lineMesh->Render();
	}

	void DrawSelectedMeshBoundingBox(CCamera* camera)
	{
		CMeshBuffer* lineMesh = nullptr;
		CMaterial* lineMat = nullptr;
		_float3 camPos = {};
		_matrix view = XMMatrixIdentity();
		_matrix proj = XMMatrixIdentity();
		if (!BuildLineResources(camera, lineMesh, lineMat, camPos, view, proj))
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

		for (const auto& edge : edges)
		{
			_vector a = worldCorners[edge[0]];
			_vector b = worldCorners[edge[1]];
			DrawLineSegment(lineMesh, lineMat, camPos, view, proj, a, b);
		}
	}

	void DrawSelectedCameraFrustum(CCamera* camera)
	{
		CMeshBuffer* lineMesh = nullptr;
		CMaterial* lineMat = nullptr;
		_float3 camPos = {};
		_matrix view = XMMatrixIdentity();
		_matrix proj = XMMatrixIdentity();
		if (!BuildLineResources(camera, lineMesh, lineMat, camPos, view, proj))
			return;

		CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject();
		if (!selected)
			return;

		CCamera* selectedCamera = selected->GetComponent<CCamera>();
		if (!selectedCamera || selectedCamera == camera)
			return;

		_matrix selectedView = selectedCamera->GetViewMatrix();
		_matrix selectedProj = selectedCamera->GetProjectionMatrix();
		_matrix invVP = XMMatrixInverse(nullptr, selectedView * selectedProj);

		const _vector clipCorners[8] =
		{
			XMVectorSet(-1.f,  1.f, 0.f, 1.f),
			XMVectorSet( 1.f,  1.f, 0.f, 1.f),
			XMVectorSet( 1.f, -1.f, 0.f, 1.f),
			XMVectorSet(-1.f, -1.f, 0.f, 1.f),
			XMVectorSet(-1.f,  1.f, 1.f, 1.f),
			XMVectorSet( 1.f,  1.f, 1.f, 1.f),
			XMVectorSet( 1.f, -1.f, 1.f, 1.f),
			XMVectorSet(-1.f, -1.f, 1.f, 1.f)
		};

		_vector worldCorners[8] = {};
		for (_uint i = 0; i < 8; ++i)
		{
			_vector world = XMVector4Transform(clipCorners[i], invVP);
			_vector worldW = XMVectorSplatW(world);
			worldCorners[i] = XMVectorDivide(world, worldW);
		}

		constexpr _uint edges[12][2] =
		{
			{0,1}, {1,2}, {2,3}, {3,0},
			{4,5}, {5,6}, {6,7}, {7,4},
			{0,4}, {1,5}, {2,6}, {3,7}
		};

		for (const auto& edge : edges)
			DrawLineSegment(lineMesh, lineMat, camPos, view, proj, worldCorners[edge[0]], worldCorners[edge[1]]);
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
	, m_pTransparentDepthStencilState(nullptr)
	, m_pSkyBoxResterizerState(nullptr)
	, m_pMeshResterizerState(nullptr)
	, m_pUIResterizerState(nullptr)
	, m_pBlendingState(nullptr)
	, m_pNoneBlendingState(nullptr)
	, m_fPssedTime(0.f)
	, m_vTempPickMousePos({-1, -1})
	, m_bSaveRegistrationEnabled(true)
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

		if (FAILED(m_pDevice->CreateDepthStencilState(&depthTransparentDesc, &m_pTransparentDepthStencilState)))
			return E_FAIL;
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

	if (!CSceneManager::GetInstance().IsPlaying())
	{
		if (m_pEditorCamera && m_pEditorCamera->Get_GameObject() && m_pEditorCamera->Get_GameObject()->IsRecursiveActive() && m_pEditorCamera->Get_Enable())
			m_pEditorCamera->Update();

		for (TRAVERSAL_ITER(m_lCameraList, it))
		{
			if ((*it) && (*it)->Get_GameObject() && (*it)->Get_GameObject()->IsRecursiveActive() && (*it)->Get_Enable())
				(*it)->Update();
		}
	}

	CPhysics::RAYCASTHIT firstHit = {};

	if (!CSceneManager::GetInstance().IsPlayMode() &&
		!CEditor::GetInstance().IsAnimatorControllerEditorFocused() &&
		CInput::GetInstance().GetKey_Editor(CONTROL))
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
		if ((*it)->IsRecursiveActive())
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
		if ((*it)->IsRecursiveActive())
			(*it)->LateUpdate();
	}
}

void CScene::Render_Editor()
{
#ifndef _CLIENT_BUILD
	if (!m_pEditorCamera)
		return;

	m_vLightData.clear();
	const _uint maxLightCount = 63u;
	vector<_float4x4> lightInfos = {};
	lightInfos.reserve(min<_uint>(static_cast<_uint>(m_lLightList.size()), maxLightCount));

	for (TRAVERSAL_ITER(m_lLightList, it))
	{
		if (!(*it))
			continue;

		if (lightInfos.size() >= maxLightCount)
			break;

		lightInfos.push_back((*it)->To_LightInfo());
	}

	_float4x4 lightMeta = {};
	lightMeta._44 = static_cast<_float>(lightInfos.size());
	m_vLightData.push_back(XMLoadFloat4x4(&lightMeta));

	for (auto& lightInfo : lightInfos)
		m_vLightData.push_back(XMLoadFloat4x4(&lightInfo));

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
	DrawSelectedCameraFrustum(m_pEditorCamera);
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

	CPhysics::GetInstance().RenderRaycastDebugDisplay();

	m_pEditorCamera->RenderRTDebugDisplay(true);

	for (TRAVERSAL_ITER(m_lObjectList, it))
		(*it)->OnPostRender_Editor();
#endif
}

void CScene::Render_Game()
{
	m_vLightData.clear();
	const _uint maxLightCount = 63u;
	vector<_float4x4> lightInfos = {};
	lightInfos.reserve(min<_uint>(static_cast<_uint>(m_lLightList.size()), maxLightCount));

	for (TRAVERSAL_ITER(m_lLightList, it))
	{
		if (!(*it))
			continue;

		if (lightInfos.size() >= maxLightCount)
			break;

		lightInfos.push_back((*it)->To_LightInfo());
	}

	_float4x4 lightMeta = {};
	lightMeta._44 = static_cast<_float>(lightInfos.size());
	m_vLightData.push_back(XMLoadFloat4x4(&lightMeta));

	for (auto& lightInfo : lightInfos)
		m_vLightData.push_back(XMLoadFloat4x4(&lightInfo));

	CGraphicDevice::GetInstance().Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());
	ColorValue initialBackgroundColor = ColorValue::black();
	CGraphicDevice::GetInstance().Clear_BackBuffer_View(&initialBackgroundColor);
	CGraphicDevice::GetInstance().Clear_DepthStencil_View();

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

	for (TRAVERSAL_ITER(m_lCameraList, it))
	{
		CCamera* camera = *it;
		if (!camera || !camera->Get_GameObject() || !camera->Get_GameObject()->IsRecursiveActive() || !camera->Get_Enable())
			continue;

		trm.Bind_GBuffer(ctx, vp);

		switch (camera->GetClearFlags())
		{
		case CCamera::ClearFlags::Skybox:
			trm.Clear_GBuffer();
			if (m_pSkyBox)
			{
				m_pContext->RSSetState(m_pSkyBoxResterizerState);
				m_pContext->OMSetDepthStencilState(m_pSkyBoxDepthStencillState, 0);
				RenderSkyBox(camera);
			}
			break;

		case CCamera::ClearFlags::SolidColor:
			trm.Clear_GBuffer();
			break;

		case CCamera::ClearFlags::DepthOnly:
			trm.Clear_GBuffer();
			break;

		case CCamera::ClearFlags::DontClear:
		default:
			break;
		}

		m_pContext->RSSetState(m_pMeshResterizerState);
		m_pContext->OMSetDepthStencilState(m_pMeshDepthStencilState, 0);

		camera->RenderMesh();
		camera->RenderShadowDepthPass(vp);
		camera->RenderObjectIDPass(vp);
		camera->RenderLightingCombined(vp);

		CGraphicDevice::GetInstance().Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());
		if (camera->GetClearFlags() == CCamera::ClearFlags::Skybox || camera->GetClearFlags() == CCamera::ClearFlags::SolidColor)
		{
			ColorValue backgroudColor = camera->Get_BackgroundColor();
			CGraphicDevice::GetInstance().Clear_BackBuffer_View(&backgroudColor);
		}
		if (camera->GetClearFlags() != CCamera::ClearFlags::DontClear)
			CGraphicDevice::GetInstance().Clear_DepthStencil_View();

		camera->RenderDisplay();
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
	for (TRAVERSAL_ITER(m_lCanvasList, it))
		Safe_Release(*it);
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
	m_mObjectOfId.clear();
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
			m_mObjectOfId.erase(obj->m_iUniqueID);
			if (auto* parent = (*it)->Get_Transform()->Get_Parent())
				parent->RemoveChild((*it)->Get_Transform());
			Safe_Release(obj);
			it = m_lObjectList.erase(it); 
		}
		else
			++it;
	}
}

void CScene::RenderSkyBox(CCamera* _camera)
{
	if (!m_pSkyBox || !_camera)
		return;

	if (_camera->GetClearFlags() != CCamera::ClearFlags::Skybox)
		return;

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
	unordered_map<const CMeshBuffer*, wstring> sharedMeshResourceNames;
	unordered_map<const CMaterial*, wstring> sharedMaterialResourceNames;
	unordered_map<const CTexture*, wstring> sharedTextureResourceNames;

	auto resolveSharedResourceName = [](auto* resource, const wstring& fallbackName, auto& sharedNameMap) -> wstring
	{
		if (!resource)
			return fallbackName;

		auto found = sharedNameMap.find(resource);
		if (found != sharedNameMap.end())
			return found->second;

		const wstring resolvedName = fallbackName.empty() ? resource->Get_ResourceName() : fallbackName;
		sharedNameMap.emplace(resource, resolvedName);
		return resolvedName;
	};

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

	auto getComponentPersistName = [](CComponent* component) -> wstring
	{
		if (!component)
			return L"";

		if (dynamic_cast<CCamera*>(component)) return L"Camera";
		if (dynamic_cast<CLight*>(component)) return L"Light";
		if (dynamic_cast<CMeshFilter*>(component)) return L"Mesh Filter";
		if (dynamic_cast<CMeshRenderer*>(component)) return L"Mesh Renderer";
		if (dynamic_cast<CSkinnedMeshRenderer*>(component)) return L"Skinned Mesh Renderer";
		if (dynamic_cast<CAnimator*>(component)) return L"Animator";
		if (dynamic_cast<CCanvas*>(component)) return L"Canvas";
		if (dynamic_cast<CImage*>(component)) return L"Image";
		if (dynamic_cast<CText*>(component)) return L"Text";
		if (dynamic_cast<CTerrain*>(component)) return L"Terrain";
		if (dynamic_cast<CUI*>(component)) return L"UI";
		if (dynamic_cast<CRigidBody*>(component)) return L"RigidBody";
		if (dynamic_cast<CCloth*>(component)) return L"Cloth";
		if (dynamic_cast<CBoxCollider*>(component)) return L"BoxCollider";
		if (dynamic_cast<CSphereCollider*>(component)) return L"SphereCollider";
		if (dynamic_cast<CCapsuleCollider*>(component)) return L"CapsuleCollider";
		if (dynamic_cast<CMeshCollider*>(component)) return L"MeshCollider";
		return L"";
	};

	for (TRAVERSAL_ITER(m_lObjectList, it))
	{
		if (CEngineString::Contains((*it)->Get_ObjectName(), L"(Clone)"))
			continue;

		if (!(*it)->Is_SaveTarget())
			continue;

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
		info.isActive = (*it)->IsActive_Origin();
		info.objLayer = (*it)->GetLayer();
		info.isTransformStatic = (*it)->IsStatic(CGameObject::STATIC_METHOD::TransformStatic);

		if (CRigidBody* rigidBody = (*it)->GetComponent<CRigidBody>())
		{
			info.rigidBodyKinematic = rigidBody->IsKinematic();
			info.rigidBodyUseGravity = rigidBody->IsUseGravity();
			info.rigidBodyMass = rigidBody->GetMass();
			info.rigidBodyConstPositionX = rigidBody->IsConstPositionX();
			info.rigidBodyConstPositionY = rigidBody->IsConstPositionY();
			info.rigidBodyConstPositionZ = rigidBody->IsConstPositionZ();
			info.rigidBodyConstRotationX = rigidBody->IsConstRotationX();
			info.rigidBodyConstRotationY = rigidBody->IsConstRotationY();
			info.rigidBodyConstRotationZ = rigidBody->IsConstRotationZ();
		}

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

			if (!component->Is_SaveTarget())
				continue;

			if (dynamic_cast<CTransform*>(component) || dynamic_cast<CRectTransform*>(component))
				continue;

			{ const wstring compName = getComponentPersistName(component); if (!compName.empty()) info.componentNames.push_back(compName); }
		}

		if (CMeshFilter* meshFilter = (*it)->GetComponent<CMeshFilter>())
		{
			if (CMeshBuffer* meshBuffer = meshFilter->Get_MeshBuffer())
			{
				wstring meshResourceName = meshBuffer->Get_ResourceName();
				wstring meshBundleName = L"";
				_int meshBundleIndex = -1;

				auto findBundleName = [&](const auto& bundleMap)
				{
					for (const auto& bundlePair : bundleMap)
					{
						for (_uint bundleIndex = 0; bundleIndex < bundlePair.second.size(); ++bundleIndex)
						{
							const MeshBundle& bundle = bundlePair.second[bundleIndex];
							if (bundle.meshBuffer == meshBuffer)
							{
								meshBundleName = bundlePair.first;
								meshBundleIndex = static_cast<_int>(bundleIndex);
								return;
							}
						}
					}
				};

				findBundleName(m_mMeshBundleList);
				if (meshBundleName.empty())
					findBundleName(m_mTempMeshBundleList);

				if (!meshBundleName.empty() && meshBundleIndex >= 0)
					meshResourceName = meshBundleName + L"::#" + to_wstring(meshBundleIndex);
				else if (!meshBundleName.empty())
					meshResourceName = meshBundleName + L"::" + meshResourceName;

				info.meshBufferName = resolveSharedResourceName(meshBuffer, meshResourceName, sharedMeshResourceNames);
			}
		}

		if (CMeshRenderer* meshRenderer = (*it)->GetComponent<CMeshRenderer>())
		{
			if (CMaterial* material = meshRenderer->Get_Material())
			{
				info.materialName = resolveSharedResourceName(material, material->Get_ResourceName(), sharedMaterialResourceNames);

				const _uint textureCount = material->Get_TextureCount();
				info.materialTextures.reserve(textureCount);
				for (_uint textureIndex = 0; textureIndex < textureCount; ++textureIndex)
				{
					CScene::SCENETRANSFORMINFO::MATERIALTEXTUREINFO textureInfo = {};
					if (CTexture* texture = material->Get_Texture(static_cast<_int>(textureIndex)))
					{
						textureInfo.name = resolveSharedResourceName(texture, texture->Get_ResourceName(), sharedTextureResourceNames);
						textureInfo.path = texture->Get_FilePath();
					}
					info.materialTextures.push_back(move(textureInfo));
				}

				for (const auto& [key, value] : material->Get_FloatValues())
					info.materialFloatValues.push_back({ key, value });
				for (const auto& [key, value] : material->Get_IntValues())
					info.materialIntValues.push_back({ key, value });
				for (const auto& [key, value] : material->Get_Vector2Values())
					info.materialVector2Values.push_back({ key, value });
				for (const auto& [key, value] : material->Get_Vector3Values())
					info.materialVector3Values.push_back({ key, value });
				for (const auto& [key, value] : material->Get_Vector4Values())
					info.materialVector4Values.push_back({ key, value });
				for (const auto& [key, value] : material->Get_MatrixValues())
					info.materialMatrixValues.push_back({ key, value });
			}
		}

		if (!(*it)->m_bIsBoneTransform)
			result.push_back(info);
	}

	return result;
}

void CScene::Bind_ObjectsTransform(const vector<SCENETRANSFORMINFO> _infoList)
{
	auto hasComponentByName = [](CGameObject* obj, const wstring& componentName) -> _bool
	{
		if (!obj)
			return false;

		for (CComponent* component : obj->Get_ComponentList())
		{
			if (!component)
				continue;

			wstring name = component->Get_UName();
			if (name == componentName)
				return true;

			if (componentName == L"Mesh Filter" && dynamic_cast<CMeshFilter*>(component)) return true;
			if (componentName == L"Mesh Renderer" && dynamic_cast<CMeshRenderer*>(component)) return true;
			if (componentName == L"Skinned Mesh Renderer" && dynamic_cast<CSkinnedMeshRenderer*>(component)) return true;
			if (componentName == L"Animator" && dynamic_cast<CAnimator*>(component)) return true;
			if (componentName == L"Camera" && dynamic_cast<CCamera*>(component)) return true;
			if (componentName == L"Light" && dynamic_cast<CLight*>(component)) return true;
			if (componentName == L"Canvas" && dynamic_cast<CCanvas*>(component)) return true;
			if (componentName == L"Image" && dynamic_cast<CImage*>(component)) return true;
			if (componentName == L"Text" && dynamic_cast<CText*>(component)) return true;
			if (componentName == L"Terrain" && dynamic_cast<CTerrain*>(component)) return true;
			if (componentName == L"UI" && dynamic_cast<CUI*>(component)) return true;
			if (componentName == L"RigidBody" && dynamic_cast<CRigidBody*>(component)) return true;
			if (componentName == L"Cloth" && dynamic_cast<CCloth*>(component)) return true;
			if (componentName == L"BoxCollider" && dynamic_cast<CBoxCollider*>(component)) return true;
			if (componentName == L"SphereCollider" && dynamic_cast<CSphereCollider*>(component)) return true;
			if (componentName == L"CapsuleCollider" && dynamic_cast<CCapsuleCollider*>(component)) return true;
			if (componentName == L"MeshCollider" && dynamic_cast<CMeshCollider*>(component)) return true;
		}

		return false;
	};

	auto ensureComponentByName = [&](CGameObject* obj, const wstring& componentName)
	{
		if (!obj || componentName.empty() || hasComponentByName(obj, componentName))
			return;

		if (componentName == L"Mesh Filter") obj->AddComponent<CMeshFilter>();
		else if (componentName == L"Mesh Renderer") obj->AddComponent<CMeshRenderer>();
		else if (componentName == L"Skinned Mesh Renderer") obj->AddComponent<CSkinnedMeshRenderer>();
		else if (componentName == L"Animator") obj->AddComponent<CAnimator>();
		else if (componentName == L"Camera") obj->AddComponent<CCamera>();
		else if (componentName == L"Light") obj->AddComponent<CLight>();
		else if (componentName == L"Canvas") obj->AddComponent<CCanvas>();
		else if (componentName == L"Image") obj->AddComponent<CImage>();
		else if (componentName == L"Text") obj->AddComponent<CText>();
		else if (componentName == L"Terrain") obj->AddComponent<CTerrain>();
		else if (componentName == L"UI") obj->AddComponent<CUI>();
		else if (componentName == L"RigidBody") obj->AddComponent<CRigidBody>();
		else if (componentName == L"Cloth") obj->AddComponent<CCloth>();
		else if (componentName == L"BoxCollider") obj->AddComponent<CBoxCollider>();
		else if (componentName == L"SphereCollider") obj->AddComponent<CSphereCollider>();
		else if (componentName == L"CapsuleCollider") obj->AddComponent<CCapsuleCollider>();
		else if (componentName == L"MeshCollider") obj->AddComponent<CMeshCollider>();
	};

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

		obj->SetActive(info.isActive);
		obj->SetLayer(info.objLayer);
		obj->SetStatic(CGameObject::STATIC_METHOD::TransformStatic, info.isTransformStatic, false);

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

	auto applyComponents = [&](CGameObject* obj, const SCENETRANSFORMINFO& info)
	{
		for (const wstring& componentName : info.componentNames)
			ensureComponentByName(obj, componentName);

		if (CRigidBody* rigidBody = obj->GetComponent<CRigidBody>())
		{
			rigidBody->SetKinematic(info.rigidBodyKinematic);
			rigidBody->SetUseGravity(info.rigidBodyUseGravity);
			rigidBody->SetMass(info.rigidBodyMass);
			rigidBody->SetConstPositionX(info.rigidBodyConstPositionX);
			rigidBody->SetConstPositionY(info.rigidBodyConstPositionY);
			rigidBody->SetConstPositionZ(info.rigidBodyConstPositionZ);
			rigidBody->SetConstRotationX(info.rigidBodyConstRotationX);
			rigidBody->SetConstRotationY(info.rigidBodyConstRotationY);
			rigidBody->SetConstRotationZ(info.rigidBodyConstRotationZ);
		}

		if (!info.meshBufferName.empty())
		{
			if (CMeshFilter* meshFilter = obj->GetComponent<CMeshFilter>())
			{
				auto findMeshInBundles = [&](const wstring& bundleName, const wstring& meshName, _int meshIndex) -> CMeshBuffer*
				{
					auto findMeshInStaticBundleList = [&](const auto& bundleList) -> CMeshBuffer*
					{
						if (meshIndex >= 0)
						{
							if (meshIndex < static_cast<_int>(bundleList.size()))
								return bundleList[meshIndex].meshBuffer;
							return nullptr;
						}

						for (const MeshBundle& bundle : bundleList)
						{
							CMeshBuffer* bundleMesh = bundle.meshBuffer;
							if (!bundleMesh)
								continue;

							if (meshName.empty() || bundleMesh->Get_ResourceName() == meshName)
								return bundleMesh;
						}

						return nullptr;
					};

					auto findMeshInSkinnedBundleList = [&](const auto& bundleList) -> CMeshBuffer*
					{
						if (meshIndex >= 0)
						{
							if (meshIndex < static_cast<_int>(bundleList.size()))
								return bundleList[meshIndex].meshBuffer;
							return nullptr;
						}

						for (const SkinnedMeshBundle& bundle : bundleList)
						{
							CMeshBuffer* bundleMesh = bundle.meshBuffer;
							if (!bundleMesh)
								continue;

							if (meshName.empty() || bundleMesh->Get_ResourceName() == meshName)
								return bundleMesh;
						}

						return nullptr;
					};

					if (!bundleName.empty())
					{
						auto staticBundleIter = m_mMeshBundleList.find(bundleName);
						if (staticBundleIter != m_mMeshBundleList.end())
							if (CMeshBuffer* found = findMeshInStaticBundleList(staticBundleIter->second))
								return found;

						auto staticTempBundleIter = m_mTempMeshBundleList.find(bundleName);
						if (staticTempBundleIter != m_mTempMeshBundleList.end())
							if (CMeshBuffer* found = findMeshInStaticBundleList(staticTempBundleIter->second))
								return found;

						auto skinnedBundleIter = m_mSkinnedBundleList.find(bundleName);
						if (skinnedBundleIter != m_mSkinnedBundleList.end())
							if (CMeshBuffer* found = findMeshInSkinnedBundleList(skinnedBundleIter->second))
								return found;

						auto skinnedTempBundleIter = m_mTempSkinnedBundleList.find(bundleName);
						if (skinnedTempBundleIter != m_mTempSkinnedBundleList.end())
							if (CMeshBuffer* found = findMeshInSkinnedBundleList(skinnedTempBundleIter->second))
								return found;
					}
					else if (meshIndex < 0)
					{
						auto findByMeshNameInMap = [&](const auto& bundleMap) -> CMeshBuffer*
						{
							for (const auto& bundlePair : bundleMap)
							{
								if (CMeshBuffer* found = findMeshInStaticBundleList(bundlePair.second))
									return found;
							}

							return nullptr;
						};

						auto findByMeshNameInSkinnedMap = [&](const auto& bundleMap) -> CMeshBuffer*
						{
							for (const auto& bundlePair : bundleMap)
							{
								if (CMeshBuffer* found = findMeshInSkinnedBundleList(bundlePair.second))
									return found;
							}

							return nullptr;
						};

						if (CMeshBuffer* found = findByMeshNameInMap(m_mMeshBundleList))
							return found;
						if (CMeshBuffer* found = findByMeshNameInMap(m_mTempMeshBundleList))
							return found;
						if (CMeshBuffer* found = findByMeshNameInSkinnedMap(m_mSkinnedBundleList))
							return found;
						if (CMeshBuffer* found = findByMeshNameInSkinnedMap(m_mTempSkinnedBundleList))
							return found;
					}

					return nullptr;
				};

				wstring meshToken = info.meshBufferName;
				wstring bundleName = L"";
				wstring meshName = meshToken;
				_int meshIndex = -1;

				size_t separatorPos = meshToken.find(L"::");
				if (separatorPos != wstring::npos)
				{
					bundleName = meshToken.substr(0, separatorPos);
					meshName = meshToken.substr(separatorPos + 2);
					if (!meshName.empty() && meshName[0] == L'#')
					{
						meshIndex = static_cast<_int>(wcstol(meshName.substr(1).c_str(), nullptr, 10));
						meshName = L"";
					}
				}

				CMeshBuffer* meshBuffer = nullptr;
				if (meshIndex < 0)
				{
					meshBuffer = CResources::GetInstance().LoadOnScene<CMeshBuffer>(meshToken);
					if (!meshBuffer)
						meshBuffer = CResources::GetInstance().LoadOnGame<CMeshBuffer>(meshToken);
					if (!meshBuffer)
						meshBuffer = CResources::GetInstance().LoadOnScene<CMeshBuffer>(meshName);
					if (!meshBuffer)
						meshBuffer = CResources::GetInstance().LoadOnGame<CMeshBuffer>(meshName);
				}
				if (!meshBuffer)
					meshBuffer = findMeshInBundles(bundleName, meshName, meshIndex);
				if (meshBuffer)
					meshFilter->Set_MeshBuffer(meshBuffer);
			}
		}

		const _bool hasMaterialOverrides = !info.materialTextures.empty() || !info.materialFloatValues.empty() || !info.materialIntValues.empty() || !info.materialVector2Values.empty() || !info.materialVector3Values.empty() || !info.materialVector4Values.empty() || !info.materialMatrixValues.empty();

		if (!info.materialName.empty() || hasMaterialOverrides)
		{
			auto resolveMaterial = [&info]() -> CMaterial*
			{
				CResources& resources = CResources::GetInstance();
				const _bool isGBufferLit =
					info.materialName == L"G_BufferLit (Material)" ||
					info.materialName == L"G_Buffer_Lit (Material)";

				if (isGBufferLit)
				{
					if (resources.LoadOnGame<CMaterial>(L"G_BufferLit (Material)"))
					{
						if (CMaterial* clone = resources.CloneOnGame<CMaterial>(L"G_BufferLit (Material)"))
						{
							clone->Set_ResourceName(info.materialName);
							return clone;
						}
					}
				}
				else
				{
					if (resources.LoadOnScene<CMaterial>(info.materialName))
					{
						if (CMaterial* clone = resources.CloneOnScene<CMaterial>(info.materialName))
							return clone;
					}
					if (resources.LoadOnGame<CMaterial>(info.materialName))
					{
						if (CMaterial* clone = resources.CloneOnGame<CMaterial>(info.materialName))
							return clone;
					}
				}
				if (CMaterial* fallbackClone = resources.CloneOnGame<CMaterial>(L"G_BufferLit (Material)"))
				{
					fallbackClone->Set_ResourceName(info.materialName);
					return fallbackClone;
				}
				return nullptr;
			};

				auto applyMaterialData = [&info](CMaterial* material)
				{
					if (!material)
						return;

					CResources& resources = CResources::GetInstance();
					auto makeTextureResourceNameFromPath = [](const wstring& path)
					{
						std::string normalized = CEngineString::WStringToString(path);
						std::replace(normalized.begin(), normalized.end(), '\\', '/');
						const std::string fileName = std::filesystem::path(normalized).filename().string();
						const size_t pathHash = std::hash<std::string>{}(normalized);
						return CEngineString::StringToWString("SceneTexture/" + fileName + "_" + std::to_string(pathHash));
					};
					auto resolveDDSPath = [&](const wstring& sourcePath)
					{
						std::filesystem::path fsPath = std::filesystem::path(sourcePath);
						wstring extension = fsPath.extension().wstring();
						std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

						if (extension == L".dds")
							return sourcePath;
						if (extension != L".png" && extension != L".jpg" && extension != L".jpeg" && extension != L".bmp" && extension != L".tga" && extension != L".tif" && extension != L".tiff")
							return sourcePath;

						const wstring textureFolder = fsPath.parent_path().filename().wstring();
						const wstring textureNoExt = fsPath.stem().wstring();
						if (textureFolder.empty() || textureNoExt.empty())
							return sourcePath;

						const wstring ddsPath = L"BinaryAssets/TextureData/" + textureFolder + L"_" + textureNoExt + L".dds";
						if (!CResources::FileExists(ddsPath))
						{
							wstring textureSourcePath = sourcePath;
							if (textureSourcePath.rfind(L"../Assets/", 0) != 0)
								textureSourcePath = L"../Assets/" + textureSourcePath;

							if (FAILED(resources.ConvertImageToDDS(textureSourcePath)))
								return sourcePath;
						}

						return ddsPath;
					};

					if (!info.materialTextures.empty())
					{
						while (material->Get_TextureCount() > 0)
							material->Remove_Texture(static_cast<_int>(material->Get_TextureCount() - 1));

						for (_uint textureIndex = 0; textureIndex < info.materialTextures.size(); ++textureIndex)
						{
						const auto& textureInfo = info.materialTextures[textureIndex];
						CTexture* texture = nullptr;

						if (!textureInfo.path.empty())
						{
							wstring path = textureInfo.path;
							if (path.rfind(L"../Assets/", 0) == 0)
								path = path.substr(10);

							path = resolveDDSPath(path);

							const wstring textureName = makeTextureResourceNameFromPath(path);
							if (CScene* crtScene = CSceneManager::GetInstance().Get_CrtScene())
								texture = dynamic_cast<CTexture*>(crtScene->Find_Resource(textureName));
							if (!texture)
								texture = resources.CreateSceneResource<CTexture>(textureName, path);
						}

						material->Set_Texture(texture, static_cast<_int>(textureIndex));
					}
				}

				for (const auto& [key, value] : info.materialFloatValues)
					material->Set_FloatValue(key, value);
				for (const auto& [key, value] : info.materialIntValues)
					material->Set_IntValue(key, value);
				for (const auto& [key, value] : info.materialVector2Values)
					material->Set_Vector2Value(key, value);
				for (const auto& [key, value] : info.materialVector3Values)
					material->Set_Vector3Value(key, value);
				for (const auto& [key, value] : info.materialVector4Values)
					material->Set_Vector4Value(key, value);
				for (const auto& [key, value] : info.materialMatrixValues)
					material->Set_MatrixValue(key, value);
			};

			if (CRenderer* renderer = dynamic_cast<CRenderer*>(obj->GetComponent<CMeshRenderer>()))
			{
				CMaterial* targetMaterial = renderer->Get_Material();
				if (!info.materialName.empty())
				{
					if (CMaterial* material = resolveMaterial())
					{
						renderer->Set_Material(material);
						targetMaterial = material;
					}
				}
				applyMaterialData(targetMaterial);
			}
			else if (CRenderer* skinnedRenderer = dynamic_cast<CRenderer*>(obj->GetComponent<CSkinnedMeshRenderer>()))
			{
				CMaterial* targetMaterial = skinnedRenderer->Get_Material();
				if (!info.materialName.empty())
				{
					if (CMaterial* material = resolveMaterial())
					{
						skinnedRenderer->Set_Material(material);
						targetMaterial = material;
					}
				}
				applyMaterialData(targetMaterial);
			}
		}
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
			applyComponents(obj, info);
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
			applyComponents(obj, info);
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
				applyComponents(obj, info);
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

			if (CGameObject* existingObj = findObjectByPath(info.objPath))
			{
				if (!info.objGuid.empty())
					existingObj->m_strGuid = info.objGuid;
				applyInfo(existingObj, info);
				applyComponents(existingObj, info);
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
			applyComponents(newObj, info);
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
	newObj->m_bSaveTarget = m_bSaveRegistrationEnabled;

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

void CScene::Set_Ambient(const _float _value)
{
	m_sEnviromentSettings.ambient = _value;
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

void CScene::Remove_Light(CLight* _light)
{
	if (_light)
		m_lLightList.remove(_light);
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

void CScene::Remove_Canvas(CCanvas* _canvas)
{
	if (!_canvas)
		return;

	m_lCanvasList.remove(_canvas);
	Safe_Release(_canvas);
}

HRESULT CScene::SaveScene(const wstring& _filePath)
{
	if (CSceneManager::GetInstance().IsPlayMode())
	{
		CDebug::LogError(L"SaveScene blocked during Play mode.");
		return E_FAIL;
	}

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

	constexpr const char* editorTag = "[Editor]";
	auto isEditorFormat = [&](const wstring& format)
	{
		return CEngineString::Contains(format, CEngineString::StringToWString(editorTag));
	};

	string sceneNameLine = "SceneName : " + CEngineString::WStringToString(m_strSceneName);
	string sceneSkyBoxLine = "SceneSkyBox : " + CEngineString::WStringToString(m_sEnviromentSettings.skyBox);
	string sceneAmbientLine = "SceneAmbient : " + to_string(m_sEnviromentSettings.ambient);
	string sceneDirectionalLightShadowDistLine = "SceneDirectionalLightShadowDist : " + to_string(m_sEnviromentSettings.directionalLightShadowDist);
	string sceneShadowBiasLine = "SceneShadowBias : " + to_string(m_sEnviromentSettings.shadowBias);
	vector<string> preservedManualLines;
	unordered_map<wstring, SceneResourceEntry> previousEntries;
	unordered_set<wstring> previousNonEditorClipPaths;
	{
		ifstream prev(_filePath);
		string line;
		while (getline(prev, line))
		{
			if (line.empty() || CEngineString::Contains(line, "//") || !CEngineString::Contains(line, " : "))
			{
				preservedManualLines.push_back(line);
				continue;
			}

			auto split = CEngineString::Split(line, " : ");
			if (split.size() < 2)
			{
				preservedManualLines.push_back(line);
				continue;
			}

			const string key = split[0];
			if (key == "SceneName" || key == "Scene name")
			{
				continue;
			}


			if (key == "SceneSkyBox" || key == "SceneAmbient" || key == "SceneDirectionalLightShadowDist" || key == "SceneShadowBias")
			{
				continue;
			}
			if (split.size() < 3)
			{
				preservedManualLines.push_back(line);
				continue;
			}

			SceneResourceEntry e = {};
			e.name = CEngineString::StringToWString(split[0]);
			e.path = normalizePath(CEngineString::StringToWString(split[1]));
			e.format = CEngineString::StringToWString(split[2]);
			previousEntries[e.name] = e;

			if (!isEditorFormat(e.format))
			{
				if (CEngineString::Contains(e.format, L"[Animation Clip]") && !e.path.empty())
					previousNonEditorClipPaths.insert(e.path);
				preservedManualLines.push_back(line);
			}
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

		if (!isEditorFormat(entry.format))
			entry.format += L" [Editor]";

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

			wstring normalizedPath = normalizePath(resource->Get_FilePath());
			if (!normalizedPath.empty() && previousNonEditorClipPaths.find(normalizedPath) != previousNonEditorClipPaths.end())
				return;
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

		if (!obj->Is_SaveTarget())
			continue;

		if (CEngineString::Contains(obj->Get_ObjectName(), L"(Clone)"))
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

	out << sceneNameLine << "\n";
	out << sceneSkyBoxLine << "\n";
	out << sceneAmbientLine << "\n";
	out << sceneDirectionalLightShadowDistLine << "\n";
	out << sceneShadowBiasLine << "\n";
	for (const auto& preservedLine : preservedManualLines)
		out << preservedLine << "\n";
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

const _bool CScene::Is_SaveRegistrationEnabled() const
{
	return m_bSaveRegistrationEnabled;
}

void CScene::Set_SaveRegistrationEnabled(const _bool _enabled)
{
	m_bSaveRegistrationEnabled = _enabled;
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
	auto containsToken = [](const string& value, const string& token)
	{
		return value.find(token) != string::npos;
	};

	auto trimCopy = [](string value)
	{
		auto notSpace = [](unsigned char ch)
		{
			return !isspace(ch);
		};

		auto begin = find_if(value.begin(), value.end(), notSpace);
		auto end = find_if(value.rbegin(), value.rend(), notSpace).base();

		if (begin >= end)
			return string();

		return string(begin, end);
	};


	m_sEnviromentSettings = EnviromentSettings{};
	auto reuseEditorTaggedResource = [&](const string& name, const string& format)
	{
		if (!containsToken(format, "[Editor]"))
			return false;

		if (name.empty())
			return true;

		const wstring wName = CEngineString::StringToWString(name);

		auto tryAddTempResource = [&](const wstring& resourceKey)
		{
			auto tempIter = m_mTempResourceList.find(resourceKey);
			if (tempIter != m_mTempResourceList.end())
				return true;

			auto currentIter = m_mResourceList.find(resourceKey);
			if (currentIter == m_mResourceList.end() || !currentIter->second)
				return false;

			Add_TempResource(resourceKey, currentIter->second);
			return true;
		};

		if (containsToken(format, "[Skinned Mesh]"))
		{
			const wstring key = wName + L" (MeshBuffer)";

			auto tempIter = m_mTempSkinnedBundleList.find(key);
			if (tempIter == m_mTempSkinnedBundleList.end())
			{
				auto bundleIter = m_mSkinnedBundleList.find(key);
				if (bundleIter != m_mSkinnedBundleList.end())
					Add_TempSkinnedBundle(key, bundleIter->second);
			}

			auto tempBoneIter = m_mTempSkinnedBoneList.find(key);
			if (tempBoneIter == m_mTempSkinnedBoneList.end())
			{
				auto boneIter = m_mSkinnedBoneList.find(key);
				if (boneIter != m_mSkinnedBoneList.end())
					Add_TempSkinnedMeshBone(key, boneIter->second);
			}

			return true;
		}

		if (containsToken(format, "[Mesh]"))
		{
			const wstring key = wName + L" (MeshBuffer)";

			auto tempIter = m_mTempMeshBundleList.find(key);
			if (tempIter != m_mTempMeshBundleList.end())
				return true;

			auto bundleIter = m_mMeshBundleList.find(key);
			if (bundleIter == m_mMeshBundleList.end())
				return false;

			Add_TempMeshBundle(key, bundleIter->second);
			return true;
		}

		if (containsToken(format, "[Texture]"))
			return tryAddTempResource(wName + L" (Texture)");

		if (containsToken(format, "[Animation Clip]"))
			return tryAddTempResource(wName + L" (Animation Clip)");

		if (containsToken(format, "[Animator Controller]"))
			return tryAddTempResource(wName + L" (Animator Controller)");

		return true;
	};

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
		line = trimCopy(line);

		if (line.empty())
			continue;

		if (line.rfind("//", 0) == 0)
			continue;

		if (CEngineString::Contains(line, ':'))
		{
			auto split = CEngineString::Split(line, " : ");

			if (split.size() < 2)
				continue;

			string name = "";
			string filepath = "";
			string format = "";

			name = split[0];
			filepath = split[1];

			if (split.size() >= 3)
				format = split[2];

			if (name == "SceneName" || name == "Scene name")
				continue;

			if (name == "SceneSkyBox")
			{
				m_sEnviromentSettings.skyBox = CEngineString::StringToWString(filepath);
				continue;
			}

			if (name == "SceneAmbient")
			{
				try { m_sEnviromentSettings.ambient = stof(filepath); } catch (...) {}
				continue;
			}

			if (name == "SceneDirectionalLightShadowDist")
			{
				try { m_sEnviromentSettings.directionalLightShadowDist = stof(filepath); } catch (...) {}
				continue;
			}

			if (name == "SceneShadowBias")
			{
				try { m_sEnviromentSettings.shadowBias = stof(filepath); } catch (...) {}
				continue;
			}

			if (reuseEditorTaggedResource(name, format))
				continue;

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
			else
				CEditor::GetInstance().Set_SelectedGameObject(nullptr);
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

ID3D11DepthStencilState* CScene::Get_TransparentDepthStencillState() const
{
	return m_pTransparentDepthStencilState;
}

ID3D11BlendState* CScene::Get_BlendingState() const
{
	return m_pBlendingState;
}

ID3D11BlendState* CScene::Get_NoneBlendingState() const
{
	return m_pNoneBlendingState;
}




