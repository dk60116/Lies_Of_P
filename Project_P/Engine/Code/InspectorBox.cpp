#include "epch.h"
#include "InspectorBox.h"
#include "Resources.h"
#include "Transform.h"
#include "RectTransform.h"
#include "Camera.h"
#include "Light.h"
#include "MeshRenderer.h"
#include "MeshFilter.h"
#include "SkinnedMeshRenderer.h"
#include "Animator.h"
#include "UI.h"
#include "Canvas.h"
#include "Terrain.h"
#include "Image.h"
#include "Text.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "RigidBody.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_set>

static vector<string> CollectRelativeFilesByExtension(const fs::path& root, const string& extension)
{
    vector<string> files;
    error_code ec;

    if (!fs::exists(root, ec))
        return files;

    const string extLower = CEditor::ToLowerCopy(extension);

    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec))
    {
        if (ec)
            continue;

        const fs::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec))
            continue;

        string fileExt = CEditor::ToLowerCopy(entry.path().extension().string());
        if (fileExt != extLower)
            continue;

        fs::path rel = entry.path().lexically_relative(root);
        string relPath = rel.empty() ? entry.path().filename().string() : rel.string();
        std::replace(relPath.begin(), relPath.end(), '\\', '/');
        files.push_back(relPath);
    }

    sort(files.begin(), files.end());
    return files;
}

struct PathTreeNode
{
    map<string, PathTreeNode> children;
    _bool isFile = false;
    string fullPath;
};

struct PendingMeshSelectionRequest
{
    CGameObject* obj = nullptr;
    CMeshFilter* meshFilter = nullptr;
    string relPath;
    _bool selectedMeshData = false;
    _bool pending = false;
};

static PendingMeshSelectionRequest g_pendingMeshSelectionRequest;

static vector<string> SplitBySlash(const string& input)
{
    vector<string> parts;
    size_t start = 0;

    while (start <= input.size())
    {
        const size_t pos = input.find('/', start);
        const size_t len = (pos == string::npos) ? input.size() - start : pos - start;
        if (len > 0)
            parts.push_back(input.substr(start, len));

        if (pos == string::npos)
            break;

        start = pos + 1;
    }

    return parts;
}

static void AddPathToTree(PathTreeNode& root, const string& relPath)
{
    vector<string> segments = SplitBySlash(relPath);
    PathTreeNode* node = &root;

    for (const string& segment : segments)
        node = &node->children[segment];

    node->isFile = true;
    node->fullPath = relPath;
}




static vector<string> g_cachedTextureFiles;
static _bool g_cachedTextureFilesInitialized = false;

static const vector<string>& GetTextureRelativeFiles(const _bool forceRefresh = false)
{
    if (!g_cachedTextureFilesInitialized || forceRefresh)
    {
        static const vector<string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".tif", ".tiff", ".gif" };
        vector<string> files;
        unordered_set<string> unique;

        for (const string& ext : extensions)
        {
            vector<string> extFiles = CollectRelativeFilesByExtension(fs::path(L"../Assets"), ext);
            for (const string& file : extFiles)
            {
                if (unique.insert(file).second)
                    files.push_back(file);
            }
        }

        sort(files.begin(), files.end());
        g_cachedTextureFiles = move(files);
        g_cachedTextureFilesInitialized = true;
    }

    return g_cachedTextureFiles;
}

static CTexture* LoadInspectorTextureResource(const string& relPath)
{
    if (relPath.empty())
        return nullptr;

    CResources& resources = CResources::GetInstance();
    string normalized = relPath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const string fileName = fs::path(normalized).filename().string();
    const size_t pathHash = std::hash<string>{}(normalized);
    const wstring resourceName = CEngineString::StringToWString("InspectorTexture/" + fileName + "_" + to_string(pathHash));

    auto found = resources.m_mGameResourceList.find(resourceName);
    if (found != resources.m_mGameResourceList.end())
        return dynamic_cast<CTexture*>(found->second);

    return resources.CreateGameResource<CTexture>(resourceName, CEngineString::StringToWString(relPath));
}

static CTexture* FindInspectorTextureResource(const string& relPath)
{
    if (relPath.empty())
        return nullptr;

    CResources& resources = CResources::GetInstance();
    string normalized = relPath;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const string fileName = fs::path(normalized).filename().string();
    const size_t pathHash = std::hash<string>{}(normalized);
    const wstring resourceName = CEngineString::StringToWString("InspectorTexture/" + fileName + "_" + to_string(pathHash));

    auto found = resources.m_mGameResourceList.find(resourceName);
    if (found == resources.m_mGameResourceList.end())
        return nullptr;

    return dynamic_cast<CTexture*>(found->second);
}

struct TexturePickerState
{
    _bool open = false;
    CMaterial* material = nullptr;
    _int slotIndex = -1;
    string selectedFolder = "All";
    string searchText;
};

static TexturePickerState g_texturePickerState;

static void OpenTexturePicker(CMaterial* material, const _int slotIndex)
{
    if (!material || slotIndex < 0)
        return;

    g_texturePickerState.open = true;
    g_texturePickerState.material = material;
    g_texturePickerState.slotIndex = slotIndex;
}

static vector<string> CollectTextureFolders(const vector<string>& files)
{
    unordered_set<string> unique;
    vector<string> folders;

    unique.insert("All");
    folders.push_back("All");

    for (const string& file : files)
    {
        string folder = fs::path(file).parent_path().string();
        std::replace(folder.begin(), folder.end(), '\\', '/');
        if (folder.empty())
            continue;

        string cumulative;
        vector<string> parts = SplitBySlash(folder);
        for (const string& part : parts)
        {
            if (!cumulative.empty())
                cumulative += "/";
            cumulative += part;

            if (unique.insert(cumulative).second)
                folders.push_back(cumulative);
        }
    }

    sort(folders.begin() + 1, folders.end());
    return folders;
}

static void AddFolderPathToTree(PathTreeNode& root, const string& folderPath)
{
    if (folderPath.empty() || folderPath == "All")
        return;

    vector<string> segments = SplitBySlash(folderPath);
    PathTreeNode* node = &root;

    for (const string& segment : segments)
        node = &node->children[segment];
}

static void RenderFolderTreeRecursive(const PathTreeNode& node, const string& currentPath, string& selectedFolder)
{
    for (const auto& childPair : node.children)
    {
        const string& name = childPair.first;
        const PathTreeNode& child = childPair.second;

        string nodePath = currentPath.empty() ? name : currentPath + "/" + name;
        const bool isSelected = (selectedFolder == nodePath);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (child.children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx(nodePath.c_str(), flags, "%s", name.c_str());
        if (ImGui::IsItemClicked())
            selectedFolder = nodePath;

        if (opened)
        {
            if (!child.children.empty())
                RenderFolderTreeRecursive(child, nodePath, selectedFolder);
            ImGui::TreePop();
        }
    }
}


static string BuildShortLabel(const string& input, const size_t maxLen)
{
    if (input.size() <= maxLen)
        return input;

    if (maxLen <= 3)
        return input.substr(0, maxLen);

    return input.substr(0, maxLen - 3) + "...";
}

static void RenderTexturePickerWindow()
{
    if (!g_texturePickerState.open)
        return;

    ImGui::SetNextWindowSize(ImVec2(900.f, 600.f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Texture Picker", &g_texturePickerState.open))
    {
        ImGui::End();
        return;
    }

    if (!g_texturePickerState.material || g_texturePickerState.slotIndex < 0)
    {
        g_texturePickerState.open = false;
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh List"))
        GetTextureRelativeFiles(true);

    const vector<string>& textureFiles = GetTextureRelativeFiles();
    vector<string> folders = CollectTextureFolders(textureFiles);

    if (g_texturePickerState.selectedFolder.empty())
        g_texturePickerState.selectedFolder = "All";

    ImGui::InputText("Search", &g_texturePickerState.searchText);

    ImGui::Separator();

    ImGui::BeginChild("##TextureFolderPane", ImVec2(230.f, 0.f), true);
    if (ImGui::Selectable("All", g_texturePickerState.selectedFolder == "All"))
        g_texturePickerState.selectedFolder = "All";

    PathTreeNode folderRoot;
    for (const string& folderPath : folders)
        AddFolderPathToTree(folderRoot, folderPath);

    RenderFolderTreeRecursive(folderRoot, "", g_texturePickerState.selectedFolder);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##TextureThumbnailPane", ImVec2(0.f, 0.f), true);

    const string searchFilter = CEditor::ToLowerCopy(g_texturePickerState.searchText);

    const float thumbnailSize = 72.f;
    const float cellWidth = 120.f;
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    const _int columns = max(1, static_cast<_int>(panelWidth / cellWidth));

    vector<string> filteredFiles;
    for (const string& relPath : textureFiles)
    {
        string normalizedPath = relPath;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        const string lowerPath = CEditor::ToLowerCopy(normalizedPath);
        string parentFolder = fs::path(normalizedPath).parent_path().string();
        std::replace(parentFolder.begin(), parentFolder.end(), '\\', '/');

        if (g_texturePickerState.selectedFolder != "All")
        {
            if (parentFolder != g_texturePickerState.selectedFolder && parentFolder.rfind(g_texturePickerState.selectedFolder + "/", 0) != 0)
                continue;
        }

        if (!searchFilter.empty() && lowerPath.find(searchFilter) == string::npos)
            continue;

        filteredFiles.push_back(relPath);
    }

    if (ImGui::BeginTable("##TextureGrid", columns))
    {
        const _int rowCount = static_cast<_int>((filteredFiles.size() + columns - 1) / columns);
        ImGuiListClipper clipper;
        clipper.Begin(rowCount);

        while (clipper.Step())
        {
            for (_int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                ImGui::TableNextRow();

                for (_int col = 0; col < columns; ++col)
                {
                    const _int index = row * columns + col;
                    ImGui::TableSetColumnIndex(col);

                    if (index >= static_cast<_int>(filteredFiles.size()))
                        continue;

                    const string& relPath = filteredFiles[index];
                    ImGui::PushID(relPath.c_str());

                    CTexture* texture = FindInspectorTextureResource(relPath);
                    if (!texture)
                        texture = LoadInspectorTextureResource(relPath);
                    _bool selected = false;
                    if (texture && texture->Get_SRV())
                        selected = ImGui::ImageButton("##TexThumb", ImTextureRef((ImTextureID)(intptr_t)texture->Get_SRV()), ImVec2(thumbnailSize, thumbnailSize));
                    else
                        selected = ImGui::Button("Select", ImVec2(thumbnailSize, thumbnailSize));

                    if (selected)
                    {
                        g_texturePickerState.material->Set_Texture(texture, g_texturePickerState.slotIndex);
                        g_texturePickerState.open = false;
                        ImGui::PopID();
                        clipper.End();
                        ImGui::EndTable();
                        ImGui::EndChild();
                        ImGui::End();
                        return;
                    }

                    const string fileName = fs::path(relPath).filename().string();
                    const string shortName = BuildShortLabel(fileName, 12);
                    ImGui::TextUnformatted(shortName.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", fileName.c_str());

                    ImGui::PopID();
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::End();
}

static string NormalizeSlashPath(const string& path)
{
    string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
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

static _bool EnsureSceneMeshEntry(const fs::path& scenePath, const string& assetRelPath, string& outEntryName)
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
        if (parts.size() < 2)
            continue;

        const string filePath = NormalizeSlashPath(parts[1]);
        if (filePath != normalizedTarget)
            continue;

        outEntryName = parts[0];
        return true;
    }

    in.close();

    outEntryName = MakeUniqueSceneEntryName(BuildMeshDataBaseName(normalizedTarget));

    ofstream out(scenePath, ios::app);
    if (!out.is_open())
        return false;

    out << outEntryName << " : " << normalizedTarget << " : [Mesh]" << "\n";
    return true;
}

static _bool RemoveFirstMeshRendererComponent(CGameObject* obj)
{
    if (!obj)
        return false;

    list<CComponent*>& components = obj->Get_ComponentList();
    for (auto it = components.begin(); it != components.end(); ++it)
    {
        if (CMeshRenderer* meshRenderer = dynamic_cast<CMeshRenderer*>(*it))
        {
            components.erase(it);
            meshRenderer->OnDestroy();
            Safe_Release(meshRenderer);
            return true;
        }
    }

    return false;
}


static _bool RemoveSpecificMeshFilterComponent(CGameObject* obj, CMeshFilter* meshFilter)
{
    if (!obj || !meshFilter)
        return false;

    list<CComponent*>& components = obj->Get_ComponentList();
    for (auto it = components.begin(); it != components.end(); ++it)
    {
        if ((*it) == meshFilter)
        {
            components.erase(it);
            meshFilter->OnDestroy();
            Safe_Release(meshFilter);
            return true;
        }
    }

    return false;
}


static vector<MeshBundle> EnsureSceneMeshBundles(const wstring& sceneMeshResourceName, const string& meshDataFile)
{
    vector<MeshBundle> bundles = CResources::GetInstance().LoadMeshBuffersOnScene(sceneMeshResourceName);
    if (!bundles.empty())
        return bundles;

    vector<CMeshBuffer::MeshBufferInitiaizeInfo> meshInfos = CResources::GetInstance().ReadMeshBufferInfos(CEngineString::StringToWString(meshDataFile));
    if (meshInfos.empty())
        return {};

    CResources::GetInstance().CreateSceneMeshBundle(sceneMeshResourceName, meshInfos, FILTER_MESHBUFFER | FILTER_MATERIAL, nullptr, false);
    return CResources::GetInstance().LoadMeshBuffersOnScene(sceneMeshResourceName);
}

static void ApplyMeshSelectionToObject(CGameObject* obj, CMeshFilter* meshFilter, const string& relPath, const _bool selectedMeshData)
{
    if (!obj || !meshFilter)
        return;

    CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
    if (!scene)
        return;

    const string normalizedRelPath = NormalizeSlashPath(relPath);
    string sceneEntryName = selectedMeshData ? fs::path(normalizedRelPath).stem().string() : string();

    if (!selectedMeshData)
    {
        const string ext = CEditor::ToLowerCopy(fs::path(normalizedRelPath).extension().string());
        const fs::path assetPath = fs::path("../Assets") / fs::path(normalizedRelPath);
        if (ext != ".fbx" || !fs::exists(assetPath))
            return;

        const fs::path scenePath = fs::path("../Assets/Scenes") / (CEngineString::WStringToString(scene->Get_SceneName()) + ".scene");
        if (!EnsureSceneMeshEntry(scenePath, normalizedRelPath, sceneEntryName))
            return;
    }

    const string expectedMeshDataFile = sceneEntryName + ".meshdata";
    const fs::path expectedMeshDataPath = fs::path("BinaryAssets/MeshData") / expectedMeshDataFile;
    const _bool meshDataExistsInitially = fs::exists(expectedMeshDataPath);
    const wstring sceneMeshResourceName = CEngineString::StringToWString(sceneEntryName + " (MeshBuffer)");

    if (meshDataExistsInitially)
    {
        vector<MeshBundle> bundles = EnsureSceneMeshBundles(sceneMeshResourceName, expectedMeshDataFile);
        if (bundles.empty())
            return;

        meshFilter->Set_MeshBuffer(nullptr);
        RemoveFirstMeshRendererComponent(obj);
        RemoveSpecificMeshFilterComponent(obj, meshFilter);
        obj->CreateMeshHierachy(bundles, 0.01f);
        return;
    }

    if (selectedMeshData)
        return;

    CResources::GetInstance().ConvertFBXToMeshBufferData(CEngineString::StringToWString(normalizedRelPath));

    const string convertedBase = BuildMeshDataBaseName(normalizedRelPath);
    const fs::path convertedMeshDataPath = fs::path("BinaryAssets/MeshData") / (convertedBase + ".meshdata");
    if (!fs::exists(convertedMeshDataPath))
        return;

    if (convertedMeshDataPath != expectedMeshDataPath)
    {
        error_code ec;
        fs::copy_file(convertedMeshDataPath, expectedMeshDataPath, fs::copy_options::overwrite_existing, ec);
        if (ec)
            return;
    }

    vector<MeshBundle> bundles = EnsureSceneMeshBundles(sceneMeshResourceName, expectedMeshDataFile);
    if (bundles.empty())
        return;

    meshFilter->Set_MeshBuffer(nullptr);
    RemoveFirstMeshRendererComponent(obj);
    RemoveSpecificMeshFilterComponent(obj, meshFilter);
    obj->CreateMeshHierachy(bundles, 0.01f);
}

static void QueueMeshSelectionRequest(CGameObject* obj, CMeshFilter* meshFilter, const string& relPath, const _bool selectedMeshData)
{
    if (!obj || !meshFilter)
        return;

    g_pendingMeshSelectionRequest.obj = obj;
    g_pendingMeshSelectionRequest.meshFilter = meshFilter;
    g_pendingMeshSelectionRequest.relPath = relPath;
    g_pendingMeshSelectionRequest.selectedMeshData = selectedMeshData;
    g_pendingMeshSelectionRequest.pending = true;
}

static void ProcessPendingMeshSelectionRequest()
{
    if (!g_pendingMeshSelectionRequest.pending)
        return;

    PendingMeshSelectionRequest req = g_pendingMeshSelectionRequest;
    g_pendingMeshSelectionRequest = {};

    if (!req.obj || !req.meshFilter)
        return;

    ApplyMeshSelectionToObject(req.obj, req.meshFilter, req.relPath, req.selectedMeshData);
}

static void RenderPathTreeRecursive(const PathTreeNode& node, const string& idPrefix, CGameObject* obj, CMeshFilter* meshFilter, const _bool selectedMeshData)
{
    for (const auto& childPair : node.children)
    {
        const string& name = childPair.first;
        const PathTreeNode& child = childPair.second;

        if (child.children.empty() && child.isFile)
        {
            const string label = name + "##" + idPrefix + child.fullPath;
            if (ImGui::Selectable(label.c_str(), false))
                QueueMeshSelectionRequest(obj, meshFilter, child.fullPath, selectedMeshData);
            continue;
        }

        const string nodeKey = child.fullPath.empty() ? name : child.fullPath;
        const string label = name + "##" + idPrefix + nodeKey;
        if (ImGui::TreeNode(label.c_str()))
        {
            RenderPathTreeRecursive(child, idPrefix, obj, meshFilter, selectedMeshData);
            ImGui::TreePop();
        }
    }
}

static void RenderPathTreeList(const vector<string>& files, const string& idPrefix, CGameObject* obj, CMeshFilter* meshFilter, const _bool selectedMeshData, const string& emptyText)
{
    if (files.empty())
    {
        ImGui::Selectable(emptyText.c_str(), false, ImGuiSelectableFlags_Disabled);
        return;
    }

    PathTreeNode root;
    for (const string& relPath : files)
        AddPathToTree(root, relPath);

    RenderPathTreeRecursive(root, idPrefix, obj, meshFilter, selectedMeshData);
}

CInspectorBox::CInspectorBox()
	: m_fRXDrag(0.f)
    , m_fRYDrag(0.f)
    , m_fRZDrag(0.f)
    , m_pPreviewTexture(nullptr)
    , m_previewAssetPath()
{
}

CInspectorBox::~CInspectorBox()
{
	OnDestroy();
}

CInspectorBox* CInspectorBox::Create()
{
	CInspectorBox* newBox = new CInspectorBox();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"Inspector";

	return newBox;
}

void CInspectorBox::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	_float width = static_cast<_float>(editorOption.inspectorWidth);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImVec2 panelSize = ImVec2(width, viewport->Size.y - editorOption.topBarHeight);

	ImGui::SetNextWindowPos
	(
		ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + editorOption.topBarHeight),
		0,
		ImVec2(1.0f, 0.0f)
	);

	ImGui::SetNextWindowSize(panelSize);

	ImGui::Begin
    (
        CEngineString::WStringToString(m_strBoxName).c_str(),
		nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_HorizontalScrollbar
    );

    CGameObject* selectedObj = editor.Get_SelectedGameObject();

    if (selectedObj)
    {
        _bool active = selectedObj->IsActive();

        _float baseY = ImGui::GetCursorPosY();

        Toggle_Begin();

        if (ImGui::Checkbox("##ActiveToggle", &active))
            selectedObj->SetActive(active);

        CGameObject* s_NameTarget = nullptr;
        string  s_EditName;
        if (s_NameTarget != selectedObj)
        {
            s_NameTarget = selectedObj;
            s_EditName = CEngineString::WStringToString(selectedObj->Get_ObjectName());
        }

        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetCursorPosY(baseY);
        ImGui::Text(("[" + to_string(selectedObj->Get_UniqueID()) + "] ").c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosY(baseY + 3.f);
        ImGui::SetNextItemWidth(140.0f);
        string label = "##ObjName" + to_string(selectedObj->Get_UniqueID());
        if (ImGui::InputText(label.c_str(), &s_EditName,
            ImGuiInputTextFlags_AutoSelectAll |
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            wstring targetName = CEngineString::StringToWString(s_EditName);
            selectedObj->Set_ObjectName(targetName);
        }

        Toggle_End();

        if (!selectedObj)
            return;

        ImGui::Text(selectedObj->IsBoneTransform() ? "Bone" : "");

        if (!selectedObj->GetComponent<CRectTransform>())
            ShowTransform(selectedObj);
        else
            ShowRectTransform(selectedObj);

        ShowComponents(selectedObj);
        ProcessPendingMeshSelectionRequest();
    }
    else
        ImGui::Text("No object selected.");

    if (!selectedObj)
    {
        RenderSelectedAssetInfo(editor.Get_SelectedAssetPath());
        RenderSelectedAssetPreview(editor.Get_SelectedAssetPath());
    }

    RenderTexturePickerWindow();

	ImGui::End();
}

void CInspectorBox::OnDestroy()
{
}

void CInspectorBox::ShowTransform(CGameObject* _obj)
{
    CTransform* transform = _obj->Get_Transform();

    if (transform)
    {
        ImGui::Text("Transform");

        const _float LabelWidth = 60.f;
        const _float boxWidth = 34.f;

        // Position
        _float3 position = transform->Get_LocalPosition();
        if (ImGui::BeginTable("Position Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            ImGui::TableSetColumnIndex(1);
            
            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &position.x, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &position.y, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &position.z, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        // Rotation
        _float3 rotation = transform->Get_LocalEulerAngles();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Rotation");
            ImGui::TableSetColumnIndex(1);

            const _float rotationDeg = 2.5f;

            // X
            _float prevX = m_fRXDrag;
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &rotation.x, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            _float prevY = m_fRYDrag;
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Z
            _float prevZ = m_fRZDrag;
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            prevX = 0.f;
            prevY = 0.f;
            prevZ = 0.f;

            ImGui::EndTable();
        }

        // Scale
        _float3 scale = transform->Get_LocalScale();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &scale.x, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &scale.y, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &scale.z, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }
    }
}

void CInspectorBox::ShowRectTransform(CGameObject* _obj)
{
    CRectTransform* rectTransform = dynamic_cast<CRectTransform*>(_obj->Get_Transform());

    if (rectTransform)
    {
        ImGui::Text("Rect Transform");

        const _float LabelWidth = 0.f;
        const _float boxWidth = 34.f;

        vector2 position = rectTransform->Get_AnchoredPosition();
        if (ImGui::BeginTable("Base Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Pos X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos X", &position.x, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Pos Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos Y", &position.y, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        _float width = rectTransform->Get_Width();
        _float height = rectTransform->Get_Height();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Width"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Width", &width, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Height"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Height", &height, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        vector2 pivot = rectTransform->Get_Pivot();
        if (ImGui::BeginTable("Pivot Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Pivot");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &pivot.x, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &pivot.y, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        CRectTransform::Anchors anchors = rectTransform->Get_Anchors();
        if (ImGui::TreeNode("Anchors"))
        {
            ImGui::Text("Min");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmin", &anchors.min.x, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymin", &anchors.min.y, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();

            ImGui::Text("Max");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmax", &anchors.max.x, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymax", &anchors.max.y, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::TreePop();
        }

        vector3 rotation = rectTransform->Get_LocalEulerAngles();
        ImGui::Text("Rotation");
        ImGui::SameLine();

        // X
        _float prevX = m_fRXDrag;
        ImGui::TextUnformatted("X");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##X", &rotation.x, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Y
        _float prevY = m_fRYDrag;
        ImGui::TextUnformatted("Y");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Z
        _float prevZ = m_fRZDrag;
        ImGui::TextUnformatted("Z");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        prevX = 0.f;
        prevY = 0.f;
        prevZ = 0.f;
    }
}


void CInspectorBox::ShowComponents(CGameObject* _obj)
{
    if (!_obj)
        return;

    ImGui::Separator();
    ImGui::Text("Components");

    list<CComponent*>& components = _obj->Get_ComponentList();
    for (CComponent* component : components)
    {
        if (!component)
            continue;

        if (dynamic_cast<CTransform*>(component) || dynamic_cast<CRectTransform*>(component))
            continue;

        string componentName = CEngineString::WStringToString(component->Get_UName());
        if (componentName.empty())
            continue;

        const string headerLabel = componentName + "##" + to_string(reinterpret_cast<uintptr_t>(component));
        if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (CMeshRenderer* meshRenderer = dynamic_cast<CMeshRenderer*>(component))
                RenderMeshRendererComponent(meshRenderer);

            if (CMeshFilter* meshFilter = dynamic_cast<CMeshFilter*>(component))
                RenderMeshFilterComponent(_obj, meshFilter);

            if (CBoxCollider* boxCollider = dynamic_cast<CBoxCollider*>(component))
            {
                _bool isTrigger = boxCollider->IsTrigger();
                if (ImGui::Checkbox("Is Trigger", &isTrigger))
                    boxCollider->SetTrigger(isTrigger);

                vector3 center = boxCollider->GetCenter();
                _float centerValues[3] = { center.x, center.y, center.z };
                if (ImGui::InputFloat3("Center", centerValues))
                    boxCollider->SetCenter(vector3(centerValues[0], centerValues[1], centerValues[2]));

                vector3 size = boxCollider->GetSize();
                _float sizeValues[3] = { size.x, size.y, size.z };
                if (ImGui::InputFloat3("Size", sizeValues))
                    boxCollider->SetSize(vector3(sizeValues[0], sizeValues[1], sizeValues[2]));

                vector3 previewCenter = boxCollider->GetCenter();
                vector3 previewSize = boxCollider->GetSize();
                ImGui::Separator();
                ImGui::TextUnformatted("Preview");
                ImGui::Text("Center: (%.2f, %.2f, %.2f)", previewCenter.x, previewCenter.y, previewCenter.z);
                ImGui::Text("Size: (%.2f, %.2f, %.2f)", previewSize.x, previewSize.y, previewSize.z);
                ImGui::Text("Is Trigger: %s", boxCollider->IsTrigger() ? "True" : "False");
            }

            if (CSphereCollider* sphereCollider = dynamic_cast<CSphereCollider*>(component))
            {
                _bool isTrigger = sphereCollider->IsTrigger();
                if (ImGui::Checkbox("Is Trigger", &isTrigger))
                    sphereCollider->SetTrigger(isTrigger);

                vector3 center = sphereCollider->GetCenter();
                _float centerValues[3] = { center.x, center.y, center.z };
                if (ImGui::InputFloat3("Center", centerValues))
                    sphereCollider->SetCenter(vector3(centerValues[0], centerValues[1], centerValues[2]));

                _float radius = sphereCollider->GetRadius();
                if (ImGui::InputFloat("Radius", &radius))
                    sphereCollider->SetRadius(radius);

                ImGui::Separator();
                ImGui::TextUnformatted("Preview");
                ImGui::Text("Center: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
                ImGui::Text("Radius: %.2f", sphereCollider->GetRadius());
                ImGui::Text("Is Trigger: %s", sphereCollider->IsTrigger() ? "True" : "False");
            }

            if (CCapsuleCollider* capsuleCollider = dynamic_cast<CCapsuleCollider*>(component))
            {
                _bool isTrigger = capsuleCollider->IsTrigger();
                if (ImGui::Checkbox("Is Trigger", &isTrigger))
                    capsuleCollider->SetTrigger(isTrigger);

                vector3 center = capsuleCollider->GetCenter();
                _float centerValues[3] = { center.x, center.y, center.z };
                if (ImGui::InputFloat3("Center", centerValues))
                    capsuleCollider->SetCenter(vector3(centerValues[0], centerValues[1], centerValues[2]));

                _float radius = capsuleCollider->GetRadius();
                if (ImGui::InputFloat("Radius", &radius))
                    capsuleCollider->SetRadius(radius);

                _float height = capsuleCollider->GetHeight();
                if (ImGui::InputFloat("Height", &height))
                    capsuleCollider->SetHeight(height);

                ImGui::Separator();
                ImGui::TextUnformatted("Preview");
                ImGui::Text("Center: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
                ImGui::Text("Radius: %.2f", capsuleCollider->GetRadius());
                ImGui::Text("Height: %.2f", capsuleCollider->GetHeight());
                ImGui::Text("Is Trigger: %s", capsuleCollider->IsTrigger() ? "True" : "False");
            }

            if (CRigidBody* rigidBody = dynamic_cast<CRigidBody*>(component))
            {
                _bool isKinematic = rigidBody->IsKinematic();
                if (ImGui::Checkbox("Kinematic", &isKinematic))
                    rigidBody->SetKinematic(isKinematic);

                _bool useGravity = rigidBody->IsUseGravity();
                if (ImGui::Checkbox("Use Gravity", &useGravity))
                    rigidBody->SetUseGravity(useGravity);

                _float mass = rigidBody->GetMass();
                if (ImGui::InputFloat("Mass", &mass, 0.1f, 1.f, "%.3f"))
                    rigidBody->SetMass(mass);

                _bool constPosX = rigidBody->IsConstPositionX();
                if (ImGui::Checkbox("Const Position X", &constPosX))
                    rigidBody->SetConstPositionX(constPosX);

                _bool constPosY = rigidBody->IsConstPositionY();
                if (ImGui::Checkbox("Const Position Y", &constPosY))
                    rigidBody->SetConstPositionY(constPosY);

                _bool constPosZ = rigidBody->IsConstPositionZ();
                if (ImGui::Checkbox("Const Position Z", &constPosZ))
                    rigidBody->SetConstPositionZ(constPosZ);

                _bool constRotX = rigidBody->IsConstRotationX();
                if (ImGui::Checkbox("Const Rotation X", &constRotX))
                    rigidBody->SetConstRotationX(constRotX);

                _bool constRotY = rigidBody->IsConstRotationY();
                if (ImGui::Checkbox("Const Rotation Y", &constRotY))
                    rigidBody->SetConstRotationY(constRotY);

                _bool constRotZ = rigidBody->IsConstRotationZ();
                if (ImGui::Checkbox("Const Rotation Z", &constRotZ))
                    rigidBody->SetConstRotationZ(constRotZ);
            }
        }
    }

    ShowAddComponentMenu(_obj);
}

void CInspectorBox::RenderMeshRendererComponent(CMeshRenderer* _meshRenderer)
{
    if (!_meshRenderer)
        return;

    CMaterial* material = _meshRenderer->Get_Material();
    if (material)
    {
        string materialName = CEngineString::WStringToString(material->Get_ResourceName());
        ImGui::Text("Material: %s", materialName.c_str());

        CShader* shader = material->Get_Shader();
        if (shader)
        {
            string shaderName = CEngineString::WStringToString(shader->Get_ResourceName());
            ImGui::Text("Shader: %s", shaderName.c_str());
        }
        else
        {
            ImGui::TextUnformatted("Shader: None");
        }

        if (ImGui::Button("Add"))
            material->Set_Texture(nullptr, static_cast<_int>(material->Get_TextureCount()));

        ImGui::TextUnformatted("Textures:");
        const _uint textureCount = material->Get_TextureCount();
        if (textureCount == 0)
        {
            ImGui::BulletText("None");
        }
        else
        {
            for (_uint i = 0; i < textureCount; ++i)
            {
                CTexture* texture = material->Get_Texture(static_cast<_int>(i));
                string textureName = texture ? CEngineString::WStringToString(texture->Get_ResourceName()) : "None";

                ImGui::PushID(static_cast<int>(i));
                _bool openPicker = false;

                if (texture && texture->Get_SRV())
                    openPicker = ImGui::ImageButton("##TextureThumb", ImTextureRef((ImTextureID)(intptr_t)texture->Get_SRV()), ImVec2(20.f, 20.f));
                else
                    openPicker = ImGui::Button("Select", ImVec2(52.f, 20.f));

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", textureName.c_str());

                if (openPicker)
                    OpenTexturePicker(material, static_cast<_int>(i));

                ImGui::SameLine();
                const string shortSlotName = BuildShortLabel(textureName, 18);
                ImGui::Text("[%u] %s", i, shortSlotName.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", textureName.c_str());

                ImGui::SameLine();
                if (ImGui::Button("Empty"))
                    material->Set_Texture(nullptr, static_cast<_int>(i));

                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                {
                    material->Remove_Texture(static_cast<_int>(i));
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }
        }

        if (ImGui::TreeNode("Custom Values"))
        {
            for (const auto& [key, value] : material->Get_FloatValues())
            {
                _float v = value;
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::InputFloat((keyStr + "##CustomFloat").c_str(), &v))
                    material->Set_FloatValue(key, v);
            }

            for (const auto& [key, value] : material->Get_IntValues())
            {
                _int v = value;
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::InputInt((keyStr + "##CustomInt").c_str(), &v))
                    material->Set_IntValue(key, v);
            }

            for (const auto& [key, value] : material->Get_Vector2Values())
            {
                _float2 v = value;
                _float arr[2] = { v.x, v.y };
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::InputFloat2((keyStr + "##CustomVec2").c_str(), arr))
                    material->Set_Vector2Value(key, { arr[0], arr[1] });
            }

            for (const auto& [key, value] : material->Get_Vector3Values())
            {
                _float3 v = value;
                _float arr[3] = { v.x, v.y, v.z };
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::InputFloat3((keyStr + "##CustomVec3").c_str(), arr))
                    material->Set_Vector3Value(key, { arr[0], arr[1], arr[2] });
            }

            for (const auto& [key, value] : material->Get_Vector4Values())
            {
                _float4 v = value;
                _float arr[4] = { v.x, v.y, v.z, v.w };
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::InputFloat4((keyStr + "##CustomVec4").c_str(), arr))
                    material->Set_Vector4Value(key, { arr[0], arr[1], arr[2], arr[3] });
            }

            for (const auto& [key, value] : material->Get_MatrixValues())
            {
                const string keyStr = CEngineString::WStringToString(key);
                if (ImGui::TreeNode((keyStr + "##CustomMat").c_str()))
                {
                    _float4x4 mat = value;
                    _float row1[4] = { mat._11, mat._12, mat._13, mat._14 };
                    _float row2[4] = { mat._21, mat._22, mat._23, mat._24 };
                    _float row3[4] = { mat._31, mat._32, mat._33, mat._34 };
                    _float row4[4] = { mat._41, mat._42, mat._43, mat._44 };

                    _bool changed = false;
                    changed |= ImGui::InputFloat4(("R1##" + keyStr).c_str(), row1);
                    changed |= ImGui::InputFloat4(("R2##" + keyStr).c_str(), row2);
                    changed |= ImGui::InputFloat4(("R3##" + keyStr).c_str(), row3);
                    changed |= ImGui::InputFloat4(("R4##" + keyStr).c_str(), row4);

                    if (changed)
                    {
                        _float4x4 newMat = {};
                        newMat._11 = row1[0]; newMat._12 = row1[1]; newMat._13 = row1[2]; newMat._14 = row1[3];
                        newMat._21 = row2[0]; newMat._22 = row2[1]; newMat._23 = row2[2]; newMat._24 = row2[3];
                        newMat._31 = row3[0]; newMat._32 = row3[1]; newMat._33 = row3[2]; newMat._34 = row3[3];
                        newMat._41 = row4[0]; newMat._42 = row4[1]; newMat._43 = row4[2]; newMat._44 = row4[3];
                        material->Set_MatrixValue(key, newMat);
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

    }
    else
    {
        ImGui::TextUnformatted("Material: None");
        ImGui::TextUnformatted("Shader: None");
        ImGui::TextUnformatted("Textures: None");
    }

    CMeshFilter* meshFilter = _meshRenderer->Get_MeshFilter();
    if (!meshFilter)
    {
        ImGui::TextUnformatted("MeshFilter: None");
        return;
    }

    ImGui::TextUnformatted("MeshFilter: Linked");

    CMeshBuffer* meshBuffer = meshFilter->Get_MeshBuffer();
    if (!meshBuffer)
    {
        ImGui::TextUnformatted("MeshBuffer: None");
        return;
    }

    string meshName = CEngineString::WStringToString(meshBuffer->Get_ResourceName());
    ImGui::Text("MeshBuffer: %s", meshName.c_str());
}

void CInspectorBox::RenderMeshFilterComponent(CGameObject* _obj, CMeshFilter* _meshFilter)
{
    if (!_obj || !_meshFilter)
        return;

    vector<pair<string, CMeshBuffer*>> meshOptions;
    vector<string> fbxFiles = CollectRelativeFilesByExtension(fs::path(L"../Assets"), ".fbx");
    vector<string> meshDataFiles = CollectRelativeFilesByExtension(fs::path(L"BinaryAssets"), ".meshdata");
    CResources& resources = CResources::GetInstance();

    for (auto& entry : resources.m_mGameResourceList)
    {
        CMeshBuffer* meshBuffer = dynamic_cast<CMeshBuffer*>(entry.second);
        if (!meshBuffer)
            continue;

        string resourceName = CEngineString::WStringToString(meshBuffer->Get_ResourceName());
        meshOptions.push_back({ resourceName, meshBuffer });
    }

    sort(meshOptions.begin(), meshOptions.end(), [](const auto& a, const auto& b)
    {
        return a.first < b.first;
    });

    CMeshBuffer* currentMeshBuffer = _meshFilter->Get_MeshBuffer();
    string currentName = "None";
    _int currentIndex = -1;

    for (_uint i = 0; i < meshOptions.size(); ++i)
    {
        if (meshOptions[i].second == currentMeshBuffer)
        {
            currentIndex = static_cast<_int>(i);
            currentName = meshOptions[i].first;
            break;
        }
    }

    const string comboLabel = "MeshBuffer##" + to_string(_obj->Get_UniqueID()) + "_" + to_string(reinterpret_cast<uintptr_t>(_meshFilter));

    if (ImGui::BeginCombo(comboLabel.c_str(), currentName.c_str()))
    {
        if (ImGui::Selectable("None", currentMeshBuffer == nullptr))
            _meshFilter->Set_MeshBuffer(nullptr);

        for (_uint i = 0; i < meshOptions.size(); ++i)
        {
            const bool selected = (static_cast<_int>(i) == currentIndex);
            if (ImGui::Selectable(meshOptions[i].first.c_str(), selected))
                _meshFilter->Set_MeshBuffer(meshOptions[i].second);

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::TreeNode("Assets .fbx"))
    {
        if (ImGui::BeginChild("##fbx_tree_box", ImVec2(0.f, 180.f), true))
            RenderPathTreeList(fbxFiles, "fbx_tree_", _obj, _meshFilter, false, "(No .fbx files)");
        ImGui::EndChild();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("BinaryAssets .meshdata"))
    {
        if (ImGui::BeginChild("##meshdata_tree_box", ImVec2(0.f, 180.f), true))
            RenderPathTreeList(meshDataFiles, "meshdata_tree_", _obj, _meshFilter, true, "(No .meshdata files)");
        ImGui::EndChild();
        ImGui::TreePop();
    }

}

void CInspectorBox::ShowAddComponentMenu(CGameObject* _obj)
{
    if (!_obj)
        return;

    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");

    if (!ImGui::BeginPopup("AddComponentPopup"))
        return;

    if (ImGui::MenuItem("Camera"))
    {
        if (!_obj->GetComponent<CCamera>())
            _obj->AddComponent<CCamera>();
    }

    if (ImGui::MenuItem("Light"))
    {
        if (!_obj->GetComponent<CLight>())
            _obj->AddComponent<CLight>();
    }

    if (ImGui::MenuItem("MeshRenderer"))
    {
        if (!_obj->GetComponent<CMeshRenderer>())
            _obj->AddComponent<CMeshRenderer>();
    }

    if (ImGui::MenuItem("MeshFilter"))
    {
        if (!_obj->GetComponent<CMeshFilter>())
            _obj->AddComponent<CMeshFilter>();
    }

    if (ImGui::MenuItem("SkinnedMeshRenderer"))
    {
        if (!_obj->GetComponent<CSkinnedMeshRenderer>())
            _obj->AddComponent<CSkinnedMeshRenderer>();
    }

    if (ImGui::MenuItem("Animator"))
    {
        if (!_obj->GetComponent<CAnimator>())
            _obj->AddComponent<CAnimator>();
    }

    if (ImGui::MenuItem("Canvas"))
    {
        if (!_obj->GetComponent<CCanvas>())
            _obj->AddComponent<CCanvas>();
    }

    if (ImGui::BeginMenu("UI"))
    {
        if (ImGui::MenuItem("Image"))
        {
            if (!_obj->GetComponent<CImage>())
                _obj->AddComponent<CImage>();
        }

        if (ImGui::MenuItem("Text"))
        {
            if (!_obj->GetComponent<CText>())
                _obj->AddComponent<CText>();
        }

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Terrain"))
    {
        if (!_obj->GetComponent<CTerrain>())
            _obj->AddComponent<CTerrain>();
    }

    if (ImGui::BeginMenu("Collider"))
    {
        if (ImGui::MenuItem("BoxCollider"))
        {
            if (!_obj->GetComponent<CBoxCollider>())
                _obj->AddComponent<CBoxCollider>();
        }

        if (ImGui::MenuItem("SphereCollider"))
        {
            if (!_obj->GetComponent<CSphereCollider>())
                _obj->AddComponent<CSphereCollider>();
        }

        if (ImGui::MenuItem("CapsuleCollider"))
        {
            if (!_obj->GetComponent<CCapsuleCollider>())
                _obj->AddComponent<CCapsuleCollider>();
        }

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("RigidBody"))
    {
        if (!_obj->GetComponent<CRigidBody>())
            _obj->AddComponent<CRigidBody>();
    }

    ImGui::EndPopup();
}

static _bool IsPreviewImageExtension(const fs::path& path)
{
    string ext = CEditor::ToLowerCopy(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tif" || ext == ".tiff" || ext == ".gif";
}

static _bool TryGetAssetsRelativePath(const fs::path& path, wstring& outRel)
{
    const fs::path assetsRoot = fs::path(L"../Assets");
    error_code ec;
    fs::path absolutePath = fs::weakly_canonical(path, ec);
    fs::path absoluteRoot = fs::weakly_canonical(assetsRoot, ec);
    if (ec)
    {
        absolutePath = fs::absolute(path, ec);
        absoluteRoot = fs::absolute(assetsRoot, ec);
    }

    fs::path relative = absolutePath.lexically_relative(absoluteRoot);
    if (relative.empty() || relative.native().rfind(L"..", 0) == 0)
        return false;

    outRel = relative.wstring();
    outRel = CEngineString::Replace(outRel, L"\\", L"/");
    return true;
}

static string FormatFileSize(uintmax_t bytes)
{
    constexpr const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double size = static_cast<double>(bytes);
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < 4)
    {
        size /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream oss;
    if (unitIndex == 0)
        oss << static_cast<uintmax_t>(size) << " " << units[unitIndex];
    else
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];

    return oss.str();
}

void CInspectorBox::RenderSelectedAssetInfo(const fs::path& path)
{
    if (path.empty())
        return;

    error_code ec;
    if (!fs::exists(path, ec))
        return;

    const string name = path.filename().string();
    const string fullPath = path.string();
    const uintmax_t sizeBytes = fs::is_regular_file(path, ec) ? fs::file_size(path, ec) : 0;
    const string sizeText = (ec ? string("Unknown") : FormatFileSize(sizeBytes));

    _bool hasResolution = false;
    _uint width = 0;
    _uint height = 0;

    if (IsPreviewImageExtension(path))
    {
        wstring relPath;
        if (TryGetAssetsRelativePath(path, relPath))
        {
            const wstring pathKey = path.wstring();
            if (pathKey != m_previewAssetPath)
            {
                const wstring resourceName = L"InspectorPreview:" + pathKey;
                CResources& resources = CResources::GetInstance();
                auto found = resources.m_mGameResourceList.find(resourceName);
                if (found != resources.m_mGameResourceList.end())
                    m_pPreviewTexture = dynamic_cast<CTexture*>(found->second);
                else
                    m_pPreviewTexture = resources.CreateGameResource<CTexture>(resourceName, relPath);

                m_previewAssetPath = pathKey;
            }

            if (m_pPreviewTexture && m_pPreviewTexture->Get_SRV())
            {
                const D3D11_TEXTURE2D_DESC& desc = m_pPreviewTexture->Get_TextureDesc();
                if (desc.Width > 0 && desc.Height > 0)
                {
                    width = desc.Width;
                    height = desc.Height;
                    hasResolution = true;
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Asset");

    if (ImGui::BeginTable("AssetInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Value");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Path");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", fullPath.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Size");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(sizeText.c_str());

        if (hasResolution)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Resolution");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u x %u", width, height);
        }

        ImGui::EndTable();
    }
}

void CInspectorBox::RenderSelectedAssetPreview(const fs::path& path)
{
    if (path.empty() || !IsPreviewImageExtension(path))
        return;

    wstring relPath;
    if (!TryGetAssetsRelativePath(path, relPath))
        return;

    const wstring pathKey = path.wstring();
    if (pathKey != m_previewAssetPath)
    {
        const wstring resourceName = L"InspectorPreview:" + pathKey;
        CResources& resources = CResources::GetInstance();
        auto found = resources.m_mGameResourceList.find(resourceName);
        if (found != resources.m_mGameResourceList.end())
            m_pPreviewTexture = dynamic_cast<CTexture*>(found->second);
        else
            m_pPreviewTexture = resources.CreateGameResource<CTexture>(resourceName, relPath);

        m_previewAssetPath = pathKey;
    }

    if (!m_pPreviewTexture || !m_pPreviewTexture->Get_SRV())
        return;

    const D3D11_TEXTURE2D_DESC& desc = m_pPreviewTexture->Get_TextureDesc();
    if (desc.Width == 0 || desc.Height == 0)
        return;

    ImGui::Separator();
    ImGui::Text("Preview");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f || avail.y <= 0.0f)
        return;

    float maxWidth = avail.x;
    float maxHeight = avail.y;
    float scale = std::min(maxWidth / static_cast<float>(desc.Width), maxHeight / static_cast<float>(desc.Height));
    scale = std::min(scale, 1.0f);

    ImVec2 size(static_cast<float>(desc.Width) * scale, static_cast<float>(desc.Height) * scale);
    ImTextureID texId = (ImTextureID)(intptr_t)m_pPreviewTexture->Get_SRV();
    ImGui::Image(ImTextureRef(texId), size);
}
