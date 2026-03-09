#include "epch.h"
#include "TopToolBar.h"
#include "ProjectBox.h"
#include "HierachyBox.h"
#include "InspectorBox.h"
#include "AnimatorControllerEditorBox.h"
#include "Physics.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

CEditor::CEditor()
	: m_hEditorWindow(nullptr)
	, m_mBoxList({})
	, m_sOptions({})
	, m_eControleTool(TransformControleTool::MOVE)
	, m_pSelectedGameObject(nullptr)
	, m_pMoveTargetGameObject(nullptr)
	, m_bOpenSelectedInHierarchyRequested(false)
	, m_bShowColliderGizmo(true)
	, m_bShowMeshColliderGizmo(true)
	, m_bShowNavigationGizmo(true)
	, m_pNavigationPreviewMesh(nullptr)
	, m_pNavigationPreviewMaterial(nullptr)
	, m_vCameraPos({})
	, m_vCameraQuat({})
	, m_bDoubleClicked(false)
	, m_bIsMovingCamera(false)
	, m_vCameraMoveStartPos({})
	, m_vCameraMoveTargetPos({})
	, m_fCameraMoveDuration(0.3f)
	, m_fCameraMoveProgress(0.f)
	, m_hEditorWindowIcon_Default(nullptr)
	, m_hEditorWindoIcon_Small(nullptr)
	, m_pAnimatorControllerBox(nullptr)
{
}

CEditor::~CEditor()
{
	Release();
}

CEditor& CEditor::GetInstance()
{
	static CEditor inst;
	return inst;
}

HRESULT CEditor::Initialize()
{
#ifdef _CLIENT_BUILD
	return S_OK;
#endif

	ImGuiContext* newCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(newCtx);

	m_hEditorWindow = CreateEditorWindow();

	ImGui_ImplWin32_Init(m_hEditorWindow);
	ImGui_ImplDX11_Init(CGraphicDevice::GetInstance().Get_Device(), CGraphicDevice::GetInstance().Get_Context());

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\malgun.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesKorean());

	CTopToolBar* toolbar = CTopToolBar::Create();
	if (toolbar)
	{
		m_mBoxList.emplace(L"TopTool", toolbar);
		toolbar->AddRef();
	}

	CProjectBox* projectBox = CProjectBox::Create();
	if (projectBox)
	{
		m_mBoxList.emplace(L"Project", projectBox);
		projectBox->AddRef();
	}

	CHierachyBox* hierachyBox = CHierachyBox::Create();
	if (hierachyBox)
	{
		m_mBoxList.emplace(L"Hierachy", hierachyBox);
		hierachyBox->AddRef();
	}

	CInspectorBox* inspectorBox = CInspectorBox::Create();
	if (inspectorBox)
	{
		m_mBoxList.emplace(L"Inspector", inspectorBox);
		inspectorBox->AddRef();
	}

	return S_OK;
}

void CEditor::Release()
{
#ifdef _CLIENT_BUILD
	return;
#endif

	ImGui_ImplWin32_Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();

	for (TRAVERSAL_ITER(m_mBoxList, it))
		Safe_Release((*it).second);

	m_mBoxList.clear();
	Safe_Release(m_pNavigationPreviewMesh);
	Safe_Release(m_pNavigationPreviewMaterial);
}

HWND CEditor::Get_EditorWindow()
{
	return m_hEditorWindow;
}

void CEditor::Editor_Update_Begin()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	for (TRAVERSAL_ITER(m_mBoxList, it))
		(*it).second->Render();
}

void CEditor::Editor_Update_During()
{
	ChangeControleTool();

	if (m_bIsMovingCamera)
	{
		const _float dt = CTime::GetInstance().Get_DeltaTime();
		m_fCameraMoveProgress += dt / m_fCameraMoveDuration;

		if (m_fCameraMoveProgress >= 1.f)
		{
			m_fCameraMoveProgress = 1.f;
			m_bIsMovingCamera = false;
		}

		_float t = m_fCameraMoveProgress;
		t = t * t * (3.f - 2.f * t);

		vector3 interpPos = vector3::Lerp(m_vCameraMoveStartPos, m_vCameraMoveTargetPos, t);

		CTransform* camTransform = CSceneManager::GetInstance().Get_EditorCamera()->Get_Transform();
		camTransform->Set_Position(interpPos);
	}
}

void CEditor::Editor_Update_End()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HWND CEditor::CreateEditorWindow()
{
	WNDCLASS wc = {};
	wc.lpfnWndProc = EditorWndProc;
	wc.hInstance = CDisplay::GetInstance().Get_HInstance();
	wc.lpszClassName = L"Editor";

	RegisterClass(&wc);

	RECT rc = { 0, 0, static_cast<LONG>(m_sOptions.windowWidth), static_cast<LONG>(m_sOptions.windowHeight) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	m_hEditorWindowIcon_Default = (HICON)LoadImageW
	(
		NULL,
		L"../EngineResources/Icon/Engine_Icon.ico",
		IMAGE_ICON,
		32, 32,
		LR_LOADFROMFILE | LR_DEFAULTSIZE
	);

	m_hEditorWindoIcon_Small = (HICON)LoadImageW
	(
		NULL,
		L"../EngineResources/Icon/Engine_Icon.ico",
		IMAGE_ICON,
		16, 16,
		LR_LOADFROMFILE
	);

	HWND hwnd = CreateWindowEx
	(
		0,
		wc.lpszClassName,
		L"Editor",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		nullptr,
		nullptr,
		CDisplay::GetInstance().Get_HInstance(),
		nullptr
	);

	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)m_hEditorWindoIcon_Small);

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	return hwnd;
}

LRESULT CEditor::EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_MOUSEWHEEL:
	{
#ifndef _DEBUG
#else
		short delta = GET_WHEEL_DELTA_WPARAM(wParam);
		_float normalized = static_cast<float>(delta) / WHEEL_DELTA;

		CInput::GetInstance().Get_WheelAxisRaw() += normalized;
#endif
	}
	return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CEditor::ChangeControleTool()
{
	if (!CInput::GetInstance().GetMouseButton_Editor(1))
	{
		if (CInput::GetInstance().GetKeyDown_Editor(Q))
			Change_ControleTool(TransformControleTool::VIEW);
		if (CInput::GetInstance().GetKeyDown_Editor(W))
			Change_ControleTool(TransformControleTool::MOVE);
		if (CInput::GetInstance().GetKeyDown_Editor(E))
			Change_ControleTool(TransformControleTool::ROTATE);
		if (CInput::GetInstance().GetKeyDown_Editor(R))
			Change_ControleTool(TransformControleTool::SCALE);
		if (CInput::GetInstance().GetKeyDown_Editor(T))
			Change_ControleTool(TransformControleTool::RECT);
		if (CInput::GetInstance().GetKeyDown_Editor(Y))
			Change_ControleTool(TransformControleTool::TRANSFORM);
	}
}

CEditor::EDITORWINOPTION CEditor::Get_Options() const
{
	return m_sOptions;
}

const _bool CEditor::IsColliderGizmoVisible() const
{
	return m_bShowColliderGizmo;
}

void CEditor::SetColliderGizmoVisible(const _bool visible)
{
	m_bShowColliderGizmo = visible;
}

const _bool CEditor::IsMeshColliderGizmoVisible() const
{
	return m_bShowMeshColliderGizmo;
}

void CEditor::SetMeshColliderGizmoVisible(const _bool visible)
{
	m_bShowMeshColliderGizmo = visible;
}

const _bool CEditor::IsNavigationGizmoVisible() const
{
	return m_bShowNavigationGizmo;
}

void CEditor::SetNavigationGizmoVisible(const _bool visible)
{
	m_bShowNavigationGizmo = visible;
}

void CEditor::SetNavigationPreviewTriangles(const vector<vector3>& triangles)
{
	Safe_Release(m_pNavigationPreviewMesh);

	if (triangles.size() < 3)
		return;

	if (!m_pNavigationPreviewMaterial)
	{
		m_pNavigationPreviewMaterial = CResources::GetInstance().CloneOnGame<CMaterial>(L"DefaultLineMaterial (Material)");
		if (m_pNavigationPreviewMaterial)
		{
			m_pNavigationPreviewMaterial->Set_BaseColor(_float4(0.1f, 0.45f, 1.f, 0.35f));
			m_pNavigationPreviewMaterial->AddRef();
		}
	}

	if (!m_pNavigationPreviewMaterial)
		return;

	vector<VertexTexNormalTangentBuffer> fillVertices;
	fillVertices.reserve(triangles.size());

	constexpr _float navigationPreviewOffset = 0.02f;

	_float3 minPos = _float3(triangles[0].x, triangles[0].y, triangles[0].z);
	_float3 maxPos = _float3(triangles[0].x, triangles[0].y, triangles[0].z);
	auto appendFillVertex = [&](const vector3& point, const vector3& normal)
	{
		const vector3 liftedPoint = point + normal * navigationPreviewOffset;
		const vector3 tangent = (vector3::Cross(vector3::up(), normal).lengthSq() > 0.0001f)
			? vector3::Cross(vector3::up(), normal).normalized()
			: vector3::right();
		const _float3 pos = _float3(liftedPoint.x, liftedPoint.y, liftedPoint.z);
		VertexTexNormalTangentBuffer vertex = {};
		vertex.position = pos;
		vertex.normal = _float3(normal.x, normal.y, normal.z);
		vertex.uv = _float2(0.f, 0.f);
		vertex.tangent = _float3(tangent.x, tangent.y, tangent.z);
		fillVertices.push_back(vertex);
		minPos.x = min(minPos.x, pos.x);
		minPos.y = min(minPos.y, pos.y);
		minPos.z = min(minPos.z, pos.z);
		maxPos.x = max(maxPos.x, pos.x);
		maxPos.y = max(maxPos.y, pos.y);
		maxPos.z = max(maxPos.z, pos.z);
	};

	for (size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		const vector3 a = triangles[i];
		const vector3 b = triangles[i + 1];
		const vector3 c = triangles[i + 2];

		vector3 normal = vector3::Cross(b - a, c - a);
		if (normal.lengthSq() <= 0.000001f)
			normal = vector3::up();
		else
			normal = normal.normalized();

		if (normal.y < 0.f)
			normal *= -1.f;

		appendFillVertex(a, normal);
		appendFillVertex(b, normal);
		appendFillVertex(c, normal);
	}

	if (fillVertices.empty())
		return;

	CMeshBuffer::MeshBufferInitiaizeInfo info = {};
	info.meshName = L"NavigationPreview (Mesh Buffer)";
	info.buffer.assign(
		reinterpret_cast<const uint8_t*>(fillVertices.data()),
		reinterpret_cast<const uint8_t*>(fillVertices.data()) + sizeof(VertexTexNormalTangentBuffer) * fillVertices.size());
	info.desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	info.desc.vertexSize = sizeof(VertexTexNormalTangentBuffer);
	info.desc.vertextCount = static_cast<_uint>(fillVertices.size());
	info.desc.indexCount = 0;
	info.desc.boundingBox.Center = _float3(
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f);
	info.desc.boundingBox.Extents = _float3(
		(maxPos.x - minPos.x) * 0.5f,
		(maxPos.y - minPos.y) * 0.5f,
		(maxPos.z - minPos.z) * 0.5f);

	m_pNavigationPreviewMesh = CMeshBuffer::CreateCustomMesh(info, nullptr);
}

void CEditor::ClearNavigationPreviewTriangles()
{
	Safe_Release(m_pNavigationPreviewMesh);
}

CMeshBuffer* CEditor::GetNavigationPreviewMesh() const
{
	return m_pNavigationPreviewMesh;
}

CMaterial* CEditor::GetNavigationPreviewMaterial() const
{
	return m_pNavigationPreviewMaterial;
}

const vector2Int CEditor::Get_WindowResolution() const
{
	return vector2Int(m_sOptions.windowWidth, m_sOptions.windowHeight);
}

const vector2Int CEditor::Get_ScreenResolution() const
{
	_int width = _int(m_sOptions.windowWidth - (m_sOptions.projectWidth + m_sOptions.hierachyWidth + m_sOptions.inspectorWidth));
	_int height = _int(m_sOptions.windowHeight - (m_sOptions.topBarHeight));

	return vector2Int(width, height);
}

const CEditor::TransformControleTool CEditor::Get_ControleTool() const
{
	return m_eControleTool;
}

void CEditor::Change_ControleTool(const TransformControleTool _tool)
{
	m_eControleTool = _tool;
}

const vector3 CEditor::Get_EditorCamPositon() const
{
	return m_vCameraPos;
}

const quaternion CEditor::Get_EditorCamQuaternion() const
{
	return m_vCameraQuat;
}

void CEditor::Set_EditorCamTransform(CTransform* _transform)
{
	m_vCameraPos = _transform->Get_Position();
	m_vCameraQuat = _transform->Get_Quaternion();
}

void CEditor::Set_SelectedGameObject(CGameObject* _target, _bool _openHierarchy)
{
	if (_target == m_pSelectedGameObject)
		return;

	m_pSelectedGameObject = _target;
	m_selectedAssetPath.clear();

	if (_openHierarchy && m_pSelectedGameObject)
		m_bOpenSelectedInHierarchyRequested = true;
}

_bool CEditor::Consume_OpenSelectedInHierarchyRequest()
{
	const _bool requested = m_bOpenSelectedInHierarchyRequested;
	m_bOpenSelectedInHierarchyRequested = false;
	return requested;
}

void CEditor::Set_SelectedAssetPath(const fs::path& path)
{
	m_selectedAssetPath = path;
	m_pSelectedGameObject = nullptr;
}

void CEditor::MoveTo_SelectedGameObject(CGameObject* _target)
{
	if (!_target)
	{
		m_pMoveTargetGameObject = nullptr;
		return;
	}

	if (_target == m_pMoveTargetGameObject)
		m_bDoubleClicked = !m_bDoubleClicked;
	else
		m_bDoubleClicked = false;

	_float distance = m_bDoubleClicked ? 6.f : 3.f;

	m_pMoveTargetGameObject = _target;

	_float3 targetPos = _target->Get_Transform()->Get_Position();
	CTransform& ect = *CSceneManager::GetInstance().Get_EditorCamera()->Get_Transform();

	m_vCameraMoveStartPos = ect.Get_Position();
	m_vCameraMoveTargetPos = targetPos + ect.Get_Directions().forward * -distance;

	m_fCameraMoveProgress = 0.f;
	m_bIsMovingCamera = true;
}

CGameObject* CEditor::Get_SelectedGameObject() const
{
	return m_pSelectedGameObject;
}

void CEditor::OpenAsset(const fs::path& path)
{
	std::error_code ec;
	if (path.empty())
		return;

	if (!fs::exists(path, ec) || ec)
	{
		CDebug::LogError(L"OpenAsset failed - not exists: " + path.wstring());
		return;
	}

	if (fs::is_directory(path, ec) && !ec)
	{
		OpenAssetExternal(path);
		return;
	}

	string ext = path.extension().string();
	ext = ToLowerCopy(ext);

	if (ext == ".animatorcontroller")
	{
		OpenAnimatorController(path);
		return;
	}

	OpenAssetExternal(path);
}

const _bool CEditor::IsAnimatorControllerEditorFocused() const
{
	for (const auto& [key, boxBase] : m_mBoxList)
	{
		auto* box = dynamic_cast<CAnimatorControllerEditorBox*>(boxBase);
		if (box && box->IsShortcutFocused())
			return true;
	}

	return false;
}

void CEditor::OpenAnimatorController(const fs::path& path)
{
#ifdef _CLIENT_BUILD
	return;
#endif

	error_code ec;
	if (!fs::exists(path, ec) || ec)
	{
		CDebug::LogError(L"OpenAnimatorController failed - file not exists: " + path.wstring());
		return;
	}

	const wstring key = L"S_AnimatorController:" + path.wstring();

	CAnimatorControllerEditorBox* box = nullptr;

	auto it = m_mBoxList.find(key);
	if (it != m_mBoxList.end())
	{
		box = dynamic_cast<CAnimatorControllerEditorBox*>(it->second);
		if (!box)
		{
			CDebug::LogError(L"OpenAnimatorController failed - box type mismatch: " + key);
			return;
		}

		box->Open(path);
		return;
	}

	box = CAnimatorControllerEditorBox::Create();
	if (!box)
	{
		CDebug::LogError(L"OpenAnimatorController failed - Create() returned nullptr");
		return;
	}

	m_mBoxList.emplace(key, box);
	box->AddRef();

	box->Open(path);
}

void CEditor::OpenAssetExternal(const fs::path& path)
{
#ifdef _WIN32
	HINSTANCE r = ShellExecuteW(
		nullptr,
		L"open",
		path.wstring().c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL
	);

	if ((INT_PTR)r <= 32)
	{
		CDebug::LogError(L"OpenAssetExternal failed: " + path.wstring());
	}
#else
	CDebug::LogError(L"OpenAssetExternal is not implemented on this platform.");
#endif
}











