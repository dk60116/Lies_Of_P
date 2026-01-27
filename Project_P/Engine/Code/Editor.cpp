#include "epch.h"
#include "TopToolBar.h"
#include "ProjectBox.h"
#include "HierachyBox.h"
#include "InspectorBox.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

CEditor::CEditor()
	: m_hEditorWindow(nullptr)
	, m_mBoxList({})
	, m_sOptions({})
	, m_eControleTool(TransformControleTool::MOVE)
	, m_pSelectedGameObject(nullptr)
	, m_pMoveTargetGameObject(nullptr)
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
}

HWND CEditor::Get_EditorWindow()
{
	return m_hEditorWindow;
}

void CEditor::Editor_Update_Begin()
{
	// 1. 프레임 시작
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 2. Hierarchy/Inspector 박스 렌더
	for (TRAVERSAL_ITER(m_mBoxList, it))
		(*it).second->Render();
}

void CEditor::Editor_Update_During()
{
	ChangeControleTool();

	if (m_bIsMovingCamera)
	{
		const float dt = CTime::GetInstance().Get_DeltaTime();
		m_fCameraMoveProgress += dt / m_fCameraMoveDuration;

		if (m_fCameraMoveProgress >= 1.f)
		{
			m_fCameraMoveProgress = 1.f;
			m_bIsMovingCamera = false;
		}

		float t = m_fCameraMoveProgress;
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
	wc.lpszClassName = "Editor";

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
		"Editor",
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

void CEditor::Set_SelectedGameObject(CGameObject* _target)
{
	if (_target == m_pSelectedGameObject)
		return;

	m_pSelectedGameObject = _target;

	if (m_pSelectedGameObject)
	{
		m_pSelectedGameObject = _target;
	}
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
