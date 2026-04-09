#include "epch.h"
#include "TopToolBar.h"
#include "ProjectBox.h"
#include "HierachyBox.h"
#include "InspectorBox.h"
#include "AnimatorControllerEditorBox.h"
#include "Physics.h"
#include "Camera.h"
#include "Material.h"
#include "Renderer.h"

namespace
{
	static const char* GetTransformSpaceLabel(const CEditor::TransformSpace space)
	{
		switch (space)
		{
		case CEditor::TransformSpace::WORLD:
			return "World";
		case CEditor::TransformSpace::LOCAL:
		default:
			return "Local";
		}
	}

	static fs::path ResolveEditorSettingsPath(const _bool forSave)
	{
		const fs::path candidates[] =
		{
			"../Engine/Default/EditorSettings.setting",
			"Engine/Default/EditorSettings.setting",
			"../Project_P/Engine/Default/EditorSettings.setting",
			"Project_P/Engine/Default/EditorSettings.setting"
		};

		for (const fs::path& path : candidates)
		{
			if (fs::exists(path))
				return path;
		}

		if (forSave)
			return candidates[3];

		return candidates[0];
	}

	static string NormalizeSlashPath(const string& path)
	{
		string out = path;
		std::replace(out.begin(), out.end(), '\\', '/');
		return out;
	}

	static string BuildMeshDataBaseName(const string& assetRelPath)
	{
		const string normalized = NormalizeSlashPath(assetRelPath);
		const size_t slash = normalized.find_last_of('/');
		const string fileName = (slash == string::npos) ? normalized : normalized.substr(slash + 1);
		const string folderPart = (slash == string::npos) ? string() : normalized.substr(0, slash);
		const size_t folderSlash = folderPart.find_last_of('/');
		const string folder = folderPart.empty() ? string("Root") : folderPart.substr(folderSlash == string::npos ? 0 : folderSlash + 1);
		const size_t dot = fileName.find_last_of('.');
		const string stem = (dot == string::npos) ? fileName : fileName.substr(0, dot);
		return folder + "_" + stem;
	}

	static string MakeUniqueSceneEntryName(const string& base)
	{
		const auto now = std::chrono::system_clock::now().time_since_epoch();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
		return base + "_Auto_" + to_string(ms);
	}

	static void ClearMaterialTextures(CMaterial* material)
	{
		if (!material)
			return;

		while (material->Get_TextureCount() > 0u)
			material->Remove_Texture(static_cast<_int>(material->Get_TextureCount() - 1u));
	}

	static void ClearSpawnedRendererTexturesRecursive(CGameObject* obj)
	{
		if (!obj)
			return;

		for (CComponent* component : obj->Get_ComponentList())
		{
			CRenderer* renderer = dynamic_cast<CRenderer*>(component);
			if (!renderer)
				continue;

			ClearMaterialTextures(renderer->Get_Material());
		}

		CTransform* transform = obj->GetTransform();
		if (!transform)
			return;

		for (CTransform* child : transform->Get_ChldList())
		{
			if (!child)
				continue;

			CGameObject* childObj = child->Get_GameObject();
			if (!childObj)
				continue;

			ClearSpawnedRendererTexturesRecursive(childObj);
		}
	}

	static _bool FindSceneEntryWithFormat(const fs::path& scenePath, const string& assetRelPath, const string& format, string& outEntryName)
	{
		ifstream in(scenePath);
		if (!in.is_open())
			return false;

		const string normalizedTarget = NormalizeSlashPath(assetRelPath);
		string line;

		while (getline(in, line))
		{
			if (line.empty() || CEngineString::Contains(line, "//") || !CEngineString::Contains(line, " : "))
				continue;

			vector<string> parts = CEngineString::Split(line, " : ");
			if (parts.size() < 3)
				continue;

			if (NormalizeSlashPath(parts[1]) != normalizedTarget)
				continue;

			if (parts[2] != format)
				continue;

			outEntryName = parts[0];
			return true;
		}

		return false;
	}

	static _bool EnsureSceneEntryWithFormat(const fs::path& scenePath, const string& assetRelPath, const string& format, string& outEntryName)
	{
		if (FindSceneEntryWithFormat(scenePath, assetRelPath, format, outEntryName))
			return true;

		fs::create_directories(scenePath.parent_path());
		outEntryName = MakeUniqueSceneEntryName(BuildMeshDataBaseName(assetRelPath));

		ofstream out(scenePath, ios::app);
		if (!out.is_open())
			return false;

		out << outEntryName << " : " << NormalizeSlashPath(assetRelPath) << " : " << format << "\n";
		return true;
	}

	static vector<MeshBundle> EnsureSceneMeshBundles(const wstring& sceneMeshResourceName, const string& meshDataFile, const _bool forceReload = false)
	{
		if (!forceReload)
		{
			vector<MeshBundle> bundles = CResources::GetInstance().LoadMeshBuffersOnScene(sceneMeshResourceName);
			if (!bundles.empty())
				return bundles;
		}

		vector<CMeshBuffer::MeshBufferInitiaizeInfo> meshInfos = CResources::GetInstance().ReadMeshBufferInfos(CEngineString::StringToWString(meshDataFile));
		if (meshInfos.empty())
			return {};

		CResources::GetInstance().CreateSceneMeshBundle(sceneMeshResourceName, meshInfos, FILTER_MESHBUFFER | FILTER_MATERIAL, nullptr, false);
		return CResources::GetInstance().LoadMeshBuffersOnScene(sceneMeshResourceName);
	}

	static vector<SkinnedMeshBundle> EnsureSceneSkinnedMeshBundles(const wstring& sceneMeshResourceName, const string& skinnedDataFile, const _bool forceReload = false)
	{
		if (!forceReload)
		{
			vector<SkinnedMeshBundle> bundles = CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(sceneMeshResourceName);
			if (!bundles.empty())
				return bundles;
		}

		auto skinnedInfos = CResources::GetInstance().ReadSkinnedBufferInfos(CEngineString::StringToWString(skinnedDataFile));
		if (skinnedInfos.initList.empty())
			return {};

		return CResources::GetInstance().CreateSceneSkinnedBundle(sceneMeshResourceName, skinnedInfos.initList, skinnedInfos.skeletalList, FILTER_MESHBUFFER | FILTER_MATERIAL, nullptr, false);
	}

	static _bool CopyImportedBinary(const fs::path& sourcePath, const fs::path& targetPath)
	{
		if (!fs::exists(sourcePath))
			return false;

		if (sourcePath == targetPath)
			return true;

		error_code ec;
		fs::create_directories(targetPath.parent_path(), ec);
		ec.clear();
		fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing, ec);
		return !ec;
	}

	static wstring BuildUniqueObjectName(CScene* scene, const wstring& desiredName)
	{
		if (!scene)
			return desiredName;

		unordered_set<wstring> existingNames;
		for (CGameObject* obj : scene->Get_ObjectList())
		{
			if (obj)
				existingNames.insert(obj->Get_ObjectName());
		}

		if (existingNames.find(desiredName) == existingNames.end())
			return desiredName;

		for (_uint i = 1; i < 1000000; ++i)
		{
			const wstring candidate = desiredName + L" (" + to_wstring(i) + L")";
			if (existingNames.find(candidate) == existingNames.end())
				return candidate;
		}

		return desiredName + L" (New)";
	}

	static vector3 ResolveSceneDropSpawnPosition(CCamera* editorCamera, const vector2Int& dropViewportPos)
	{
		if (!editorCamera || !editorCamera->GetTransform())
			return vector3::zero();

		const vector3 cameraPos = editorCamera->GetTransform()->Get_Position();
		const vector3 cameraForward = editorCamera->GetTransform()->Get_Directions().forward.normalized();
		const vector3 fallbackPosition = cameraPos + cameraForward * 10.f;

		const CPhysics::Ray ray = editorCamera->ScreenPointToRay_Editor(dropViewportPos, 50000.f);
		const vector<CPhysics::RAYCASTHIT> hits = CPhysics::GetInstance().Raycast(ray, 0, false);
		for (const CPhysics::RAYCASTHIT& hit : hits)
		{
			if (!hit.isHit || !hit.object)
				continue;

			if (hit.object == editorCamera->Get_GameObject())
				continue;

			return hit.hitPos + hit.hitNormal * 0.05f;
		}

		const vector3 up = vector3::up();
		const _float denom = ray.dir.dot(up);
		if (fabsf(denom) > 1e-5f)
		{
			const _float t = -ray.origin.dot(up) / denom;
			if (t > 0.f && t < ray.maxDist)
				return ray.origin + ray.dir * t;
		}

		return fallbackPosition;
	}

	static _bool SpawnFbxAssetIntoScene(const string& assetRelPath, const vector2Int& dropViewportPos, CGameObject*& outSpawnedRoot)
	{
		outSpawnedRoot = nullptr;

		CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
		if (!scene)
			return false;

		CCamera* editorCamera = scene->Get_EditorCamera();
		if (!editorCamera)
			return false;

		const string normalizedRelPath = NormalizeSlashPath(assetRelPath);
		const string extension = CEditor::ToLowerCopy(fs::path(normalizedRelPath).extension().string());
		if (extension != ".fbx")
			return false;

		const fs::path sourceAssetPath = fs::path(L"../Assets") / fs::path(normalizedRelPath);
		if (!fs::exists(sourceAssetPath))
			return false;

		const wstring relAssetPathW = CEngineString::StringToWString(normalizedRelPath);
		CResources::GetInstance().ConvertFBXToMeshBufferData(relAssetPathW);
		CResources::GetInstance().ConvertFBXToSkinnedBufferData(relAssetPathW);

		const string convertedBase = BuildMeshDataBaseName(normalizedRelPath);
		const fs::path convertedMeshDataPath = fs::path(L"BinaryAssets/MeshData") / fs::path(convertedBase + ".meshdata");
		const fs::path convertedSkinnedDataPath = fs::path(L"BinaryAssets/SkinnedMeshData") / fs::path(convertedBase + ".skinneddata");
		const fs::path scenePath = fs::path(L"../Assets/Scenes") / fs::path(CEngineString::WStringToString(scene->Get_SceneName()) + ".scene");

		string skinnedEntryName;
		const _bool hadSkinnedEntry = FindSceneEntryWithFormat(scenePath, normalizedRelPath, "[Skinned Mesh]", skinnedEntryName);
		if (!hadSkinnedEntry && fs::exists(convertedSkinnedDataPath))
		{
			if (!EnsureSceneEntryWithFormat(scenePath, normalizedRelPath, "[Skinned Mesh]", skinnedEntryName))
				return false;
		}

		_bool spawned = false;
		const vector3 spawnPosition = ResolveSceneDropSpawnPosition(editorCamera, dropViewportPos);
		const wstring desiredRootName = CEngineString::StringToWString(fs::path(normalizedRelPath).stem().string());

		if (!skinnedEntryName.empty())
		{
			const string expectedSkinnedDataFile = skinnedEntryName + ".skinneddata";
			const fs::path expectedSkinnedDataPath = fs::path(L"BinaryAssets/SkinnedMeshData") / fs::path(expectedSkinnedDataFile);

			if (fs::exists(convertedSkinnedDataPath) && !CopyImportedBinary(convertedSkinnedDataPath, expectedSkinnedDataPath))
				return false;

			vector<SkinnedMeshBundle> skinnedBundles = EnsureSceneSkinnedMeshBundles(
				CEngineString::StringToWString(skinnedEntryName + " (MeshBuffer)"),
				expectedSkinnedDataFile,
				fs::exists(convertedSkinnedDataPath));
			auto skinnedInfo = CResources::GetInstance().ReadSkinnedBufferInfos(CEngineString::StringToWString(expectedSkinnedDataFile));

			if (!skinnedBundles.empty() && !skinnedInfo.initList.empty() && !skinnedInfo.skeletalList.empty())
			{
				CGameObject* root = scene->Add_GameObject(BuildUniqueObjectName(scene, desiredRootName));
				if (!root || !root->GetTransform())
					return false;

				root->GetTransform()->Set_Position(spawnPosition);
				root->CreateSkinnedMeshHierachy(skinnedBundles, skinnedInfo.skeletalList, 1.f, vector3::zero());
				ClearSpawnedRendererTexturesRecursive(root);
				outSpawnedRoot = root;
				spawned = true;
			}
		}

		if (spawned)
			return true;

		string meshEntryName;
		if (!EnsureSceneEntryWithFormat(scenePath, normalizedRelPath, "[Mesh]", meshEntryName))
			return false;

		const string expectedMeshDataFile = meshEntryName + ".meshdata";
		const fs::path expectedMeshDataPath = fs::path(L"BinaryAssets/MeshData") / fs::path(expectedMeshDataFile);
		if (fs::exists(convertedMeshDataPath) && !CopyImportedBinary(convertedMeshDataPath, expectedMeshDataPath))
			return false;

		vector<MeshBundle> meshBundles = EnsureSceneMeshBundles(
			CEngineString::StringToWString(meshEntryName + " (MeshBuffer)"),
			expectedMeshDataFile,
			fs::exists(convertedMeshDataPath));
		if (meshBundles.empty())
			return false;

		CGameObject* root = scene->Add_GameObject(BuildUniqueObjectName(scene, desiredRootName));
		if (!root || !root->GetTransform())
		return false;

	root->GetTransform()->Set_Position(spawnPosition);
	root->CreateMeshHierachy(meshBundles, 1.f);
	ClearSpawnedRendererTexturesRecursive(root);
	outSpawnedRoot = root;
	return true;
}

	static _bool GetEditorViewToolbarRect(ImVec2& outPos, ImVec2& outSize)
	{
		const CEditor::EDITORWINOPTION options = CEditor::GetInstance().Get_Options();
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport)
			return false;

		const _float width = static_cast<_float>(CEditor::GetInstance().Get_WindowResolution().x - options.projectWidth - options.hierachyWidth - options.inspectorWidth);
		const _float height = static_cast<_float>(options.editorViewToolbarHeight);
		if (width <= 1.f || height <= 0.f)
			return false;

		outPos = ImVec2(viewport->Pos.x, viewport->Pos.y + static_cast<_float>(options.topBarHeight));
		outSize = ImVec2(width, height);
		return true;
	}

	static _bool GetEditorViewRect(ImVec2& outPos, ImVec2& outSize)
	{
		const CEditor::EDITORWINOPTION options = CEditor::GetInstance().Get_Options();
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		if (!viewport)
			return false;

		const _float width = static_cast<_float>(CEditor::GetInstance().Get_WindowResolution().x - options.projectWidth - options.hierachyWidth - options.inspectorWidth);
		const _float height = static_cast<_float>(CEditor::GetInstance().Get_WindowResolution().y - options.topBarHeight - options.editorViewToolbarHeight);
		if (width <= 1.f || height <= 1.f)
			return false;

		outPos = ImVec2(viewport->Pos.x, viewport->Pos.y + static_cast<_float>(options.topBarHeight + options.editorViewToolbarHeight));
		outSize = ImVec2(width, height);
		return true;
	}

	static void RenderEditorViewToolbar()
	{
		ImVec2 toolbarPos = {};
		ImVec2 toolbarSize = {};
		if (!GetEditorViewToolbarRect(toolbarPos, toolbarSize))
			return;

		ImGui::SetNextWindowPos(toolbarPos);
		ImGui::SetNextWindowSize(toolbarSize);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 4.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 1.f));

		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("##EditorViewToolbarPlaceholder", nullptr, flags);

		CEditor& editor = CEditor::GetInstance();
		CEditor::TransformSpace transformSpace = editor.Get_GizmoTransformSpace();

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Transform");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.f);
		if (ImGui::BeginCombo("##EditorTransformSpace", GetTransformSpaceLabel(transformSpace)))
		{
			const CEditor::TransformSpace options[] =
			{
				CEditor::TransformSpace::LOCAL,
				CEditor::TransformSpace::WORLD
			};

			for (const CEditor::TransformSpace option : options)
			{
				const _bool isSelected = transformSpace == option;
				if (ImGui::Selectable(GetTransformSpaceLabel(option), isSelected))
					editor.Set_GizmoTransformSpace(option);

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}

	static void RenderSceneAssetDropTargetOverlay()
	{
		const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
		if (!activePayload || !activePayload->IsDataType("ProjectAssetPath"))
			return;

		if (!activePayload->Data || activePayload->DataSize <= 0)
			return;

		const char* payloadPath = reinterpret_cast<const char*>(activePayload->Data);
		if (!payloadPath || payloadPath[0] == '\0')
			return;

		const string assetRelPath = NormalizeSlashPath(payloadPath);
		if (CEditor::ToLowerCopy(fs::path(assetRelPath).extension().string()) != ".fbx")
			return;

		ImVec2 scenePos = {};
		ImVec2 sceneSize = {};
		if (!GetEditorViewRect(scenePos, sceneSize))
			return;

		ImGui::SetNextWindowPos(scenePos);
		ImGui::SetNextWindowSize(sceneSize);
		ImGui::SetNextWindowBgAlpha(0.10f);

		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;

		if (!ImGui::Begin("##EditorSceneAssetDropTarget", nullptr, flags))
		{
			ImGui::End();
			return;
		}

		ImGui::InvisibleButton("##EditorSceneAssetDropTargetButton", sceneSize);

		_bool delivered = false;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ProjectAssetPath", ImGuiDragDropFlags_AcceptBeforeDelivery))
			{
				if (payload->Preview)
				{
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					const ImVec2 min = ImGui::GetItemRectMin();
					const ImVec2 max = ImGui::GetItemRectMax();
					drawList->AddRectFilled(min, max, IM_COL32(80, 150, 110, 40), 6.f);
					drawList->AddRect(min, max, IM_COL32(120, 220, 160, 220), 6.f, 0, 2.f);

					const string label = "Drop FBX to spawn in scene";
					const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
					const ImVec2 textPos = ImVec2(
						min.x + (sceneSize.x - textSize.x) * 0.5f,
						min.y + (sceneSize.y - textSize.y) * 0.5f);
					drawList->AddText(textPos, IM_COL32(230, 255, 235, 255), label.c_str());
				}

				if (payload->Delivery)
					delivered = true;
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::End();

		if (!delivered)
			return;

		CGameObject* spawnedRoot = nullptr;
		if (SpawnFbxAssetIntoScene(assetRelPath, CInput::GetInstance().GetMousePos_Editor(), spawnedRoot) && spawnedRoot)
			CEditor::GetInstance().Set_SelectedGameObject(spawnedRoot, true);
	}
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

CEditor::CEditor()
	: m_hEditorWindow(nullptr)
	, m_mBoxList({})
	, m_sOptions({})
	, m_eControleTool(TransformControleTool::MOVE)
	, m_eLastGizmoTool(TransformControleTool::MOVE)
	, m_eGizmoTransformSpace(TransformSpace::LOCAL)
	, m_pSelectedGameObject(nullptr)
	, m_pMoveTargetGameObject(nullptr)
	, m_bOpenSelectedInHierarchyRequested(false)
	, m_bShowColliderGizmo(true)
	, m_bShowMeshColliderGizmo(true)
	, m_bShowNavigationMesh(true)
	, m_bShowGameStatusWindow(false)
	, m_bHideEditorWhilePlaying(false)
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


	LoadViewSettings();

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
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	for (TRAVERSAL_ITER(m_mBoxList, it))
		(*it).second->Render();

	RenderEditorViewToolbar();
	RenderSceneAssetDropTargetOverlay();
}

void CEditor::Editor_Update_During()
{
	if (CSceneManager::GetInstance().IsPlayMode() && IsHideEditorWhilePlaying())
	{
		m_bIsMovingCamera = false;
		m_fCameraMoveProgress = 0.f;
		return;
	}

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

		CTransform* camTransform = CSceneManager::GetInstance().Get_EditorCamera()->GetTransform();
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
	if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() || IsAnimatorControllerEditorFocused())
		return;

	if (!CInput::GetInstance().GetMouseButton_Editor(1) &&
		!CInput::GetInstance().GetMouseButton_Editor(2))
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

void CEditor::LoadViewSettings()
{
	const fs::path settingsPath = ResolveEditorSettingsPath(false);
	ifstream inFile(settingsPath);
	if (!inFile.is_open())
		return;

	string line;
	while (getline(inFile, line))
	{
		line = CEngineString::Trim(line);
		if (line.empty())
			continue;

		size_t delim = line.find('=');
		if (delim == string::npos)
			continue;

		const string key = CEngineString::Trim(line.substr(0, delim));
		const string value = CEngineString::Trim(line.substr(delim + 1));
		if (key.empty() || value.empty())
			continue;

		auto parseBool = [](const string& text, _bool& outValue) -> _bool
		{
			string lower = CEditor::ToLowerCopy(text);
			if (lower == "1" || lower == "true")
			{
				outValue = true;
				return true;
			}
			if (lower == "0" || lower == "false")
			{
				outValue = false;
				return true;
			}
			return false;
		};

		if (key == "ShowCollider")
		{
			parseBool(value, m_bShowColliderGizmo);
			continue;
		}

		if (key == "ShowMeshCollider")
		{
			parseBool(value, m_bShowMeshColliderGizmo);
			continue;
		}

		if (key == "ShowNavigationMesh")
		{
			parseBool(value, m_bShowNavigationMesh);
			continue;
		}

		if (key == "ShowGameStatus")
			parseBool(value, m_bShowGameStatusWindow);
	}
}

void CEditor::SaveViewSettings() const
{
	const fs::path settingsPath = ResolveEditorSettingsPath(true);
	fs::create_directories(settingsPath.parent_path());
	ofstream outFile(settingsPath, ios::trunc);
	if (!outFile.is_open())
		return;

	outFile << "ShowCollider=" << (m_bShowColliderGizmo ? 1 : 0) << "\n";
	outFile << "ShowMeshCollider=" << (m_bShowMeshColliderGizmo ? 1 : 0) << "\n";
	outFile << "ShowNavigationMesh=" << (m_bShowNavigationMesh ? 1 : 0) << "\n";
	outFile << "ShowGameStatus=" << (m_bShowGameStatusWindow ? 1 : 0) << "\n";
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
	if (m_bShowColliderGizmo == visible)
		return;

	m_bShowColliderGizmo = visible;
	SaveViewSettings();
}

const _bool CEditor::IsMeshColliderGizmoVisible() const
{
	return m_bShowMeshColliderGizmo;
}

void CEditor::SetMeshColliderGizmoVisible(const _bool visible)
{
	if (m_bShowMeshColliderGizmo == visible)
		return;

	m_bShowMeshColliderGizmo = visible;
	SaveViewSettings();
}

const _bool CEditor::IsNavigationMeshVisible() const
{
	return m_bShowNavigationMesh;
}

void CEditor::SetNavigationMeshVisible(const _bool visible)
{
	if (m_bShowNavigationMesh == visible)
		return;

	m_bShowNavigationMesh = visible;
	SaveViewSettings();
}

const _bool CEditor::IsGameStatusWindowVisible() const
{
	return m_bShowGameStatusWindow;
}

void CEditor::SetGameStatusWindowVisible(const _bool visible)
{
	if (m_bShowGameStatusWindow == visible)
		return;

	m_bShowGameStatusWindow = visible;
	SaveViewSettings();
}

const _bool CEditor::IsHideEditorWhilePlaying() const
{
	return m_bHideEditorWhilePlaying;
}

void CEditor::SetHideEditorWhilePlaying(const _bool hide)
{
	m_bHideEditorWhilePlaying = hide;
}

const vector2Int CEditor::Get_WindowResolution() const
{
	return vector2Int(m_sOptions.windowWidth, m_sOptions.windowHeight);
}

const vector2Int CEditor::Get_ScreenResolution() const
{
	_int width = _int(m_sOptions.windowWidth - (m_sOptions.projectWidth + m_sOptions.hierachyWidth + m_sOptions.inspectorWidth));
	_int height = _int(m_sOptions.windowHeight - (m_sOptions.topBarHeight + m_sOptions.editorViewToolbarHeight));

	return vector2Int(width, height);
}

const CEditor::TransformControleTool CEditor::Get_ControleTool() const
{
	return m_eControleTool;
}

const CEditor::TransformControleTool CEditor::Get_GizmoControleTool() const
{
	switch (m_eControleTool)
	{
	case TransformControleTool::MOVE:
	case TransformControleTool::ROTATE:
	case TransformControleTool::SCALE:
		return m_eControleTool;
	default:
		return m_eLastGizmoTool;
	}
}

const CEditor::TransformSpace CEditor::Get_GizmoTransformSpace() const
{
	return m_eGizmoTransformSpace;
}

void CEditor::Change_ControleTool(const TransformControleTool _tool)
{
	m_eControleTool = _tool;

	if (_tool == TransformControleTool::MOVE ||
		_tool == TransformControleTool::ROTATE ||
		_tool == TransformControleTool::SCALE)
	{
		m_eLastGizmoTool = _tool;
	}
}

void CEditor::Set_GizmoTransformSpace(const TransformSpace _space)
{
	m_eGizmoTransformSpace = _space;
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
	{
		const bool needsSelectionSync =
			(_target == nullptr && !m_multiSelectedObjects.empty()) ||
			(_target != nullptr && (m_multiSelectedObjects.size() != 1 || m_multiSelectedObjects.front() != _target));

		if (needsSelectionSync)
		{
			m_multiSelectedObjects.clear();
			if (_target)
				m_multiSelectedObjects.push_back(_target);
		}

		if (_openHierarchy && m_pSelectedGameObject)
			m_bOpenSelectedInHierarchyRequested = true;
		return;
	}

	m_pSelectedGameObject = _target;
	m_selectedAssetPath.clear();
	m_multiSelectedObjects.clear();

	if (m_pSelectedGameObject)
		m_multiSelectedObjects.push_back(m_pSelectedGameObject);

	if (_openHierarchy && m_pSelectedGameObject)
		m_bOpenSelectedInHierarchyRequested = true;
}

void CEditor::Set_MultiSelectedObjects(const vector<CGameObject*>& _objects)
{
	m_multiSelectedObjects = _objects;
}

const vector<CGameObject*>& CEditor::Get_MultiSelectedObjects() const
{
	return m_multiSelectedObjects;
}

_bool CEditor::IsSelected(CGameObject* _obj) const
{
	if (_obj == m_pSelectedGameObject)
		return true;
	for (CGameObject* obj : m_multiSelectedObjects)
		if (obj == _obj) return true;
	return false;
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

	_float3 targetPos = _target->GetTransform()->Get_Position();
	CTransform& ect = *CSceneManager::GetInstance().Get_EditorCamera()->GetTransform();

	m_vCameraMoveStartPos = ect.Get_Position();
	m_vCameraMoveTargetPos = targetPos + ect.Get_Directions().forward * -distance;

	m_fCameraMoveProgress = 0.f;
	m_bIsMovingCamera = true;
}

CGameObject* CEditor::Get_SelectedGameObject() const
{
	return m_pSelectedGameObject;
}

CCamera* CEditor::Get_SelectedCamera() const
{
	if (!m_pSelectedGameObject)
		return nullptr;

	return m_pSelectedGameObject->GetComponent<CCamera>();
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

