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
#include "MeshCollider.h"
#include "RigidBody.h"
#include "NaviMeshAgent.h"

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

struct PendingSkinnedMeshSelectionRequest
{
    CGameObject* obj = nullptr;
    CSkinnedMeshRenderer* skinnedMeshRenderer = nullptr;
    string relPath;
    _bool selectedSkinnedData = false;
    _bool pending = false;
};

static PendingSkinnedMeshSelectionRequest g_pendingSkinnedMeshSelectionRequest;

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
    CImage* image = nullptr;
    _int slotIndex = -1;
    string selectedFolder = "All";
    string searchText;
};

static TexturePickerState g_texturePickerState;
static vector<string> g_cachedAnimatorControllerFiles;
static _bool g_cachedAnimatorControllerFilesInitialized = false;

static void OpenTexturePicker(CMaterial* material, const _int slotIndex)
{
    if (!material || slotIndex < 0)
        return;

    g_texturePickerState.open = true;
    g_texturePickerState.material = material;
    g_texturePickerState.image = nullptr;
    g_texturePickerState.slotIndex = slotIndex;
}

static void OpenTexturePicker(CImage* image)
{
    if (!image || !image->Get_Material())
        return;

    g_texturePickerState.open = true;
    g_texturePickerState.material = image->Get_Material();
    g_texturePickerState.image = image;
    g_texturePickerState.slotIndex = 0;
}

static string NormalizeAnimatorControllerName(const wstring& resourceName)
{
    string name = CEngineString::WStringToString(resourceName);
    const string suffix = " (Animator Controller)";

    if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        name.erase(name.size() - suffix.size());

    return name;
}

static const vector<string>& GetAnimatorControllerRelativeFiles(const _bool forceRefresh = false)
{
    if (!g_cachedAnimatorControllerFilesInitialized || forceRefresh)
    {
        g_cachedAnimatorControllerFiles = CollectRelativeFilesByExtension(fs::path(L"../Assets"), ".animatorcontroller");
        g_cachedAnimatorControllerFilesInitialized = true;
    }

    return g_cachedAnimatorControllerFiles;
}

static string FindAnimatorControllerRelativePath(const CAnimatorController* controller)
{
    if (!controller)
        return "";

    const string targetName = NormalizeAnimatorControllerName(controller->Get_ResourceName());
    const auto& controllerFiles = GetAnimatorControllerRelativeFiles();

    for (const string& relPath : controllerFiles)
    {
        if (fs::path(relPath).stem().string() == targetName)
            return relPath;
    }

    return "";
}

static CAnimatorController* LoadInspectorAnimatorControllerResource(const string& relPath)
{
    if (relPath.empty())
        return nullptr;

    CResources& resources = CResources::GetInstance();
    const string controllerName = fs::path(relPath).stem().string();
    const wstring controllerNameW = CEngineString::StringToWString(controllerName);

    if (CAnimatorController* loaded = resources.LoadOnScene<CAnimatorController>(controllerNameW))
        return loaded;

    if (CAnimatorController* loaded = resources.LoadOnScene<CAnimatorController>(controllerNameW + L" (Animator Controller)"))
        return loaded;

    const fs::path relPathFs(relPath);
    const string folderName = relPathFs.parent_path().filename().string();
    const string acDataPath = folderName.empty() ? (controllerName + ".acdata") : (folderName + "_" + controllerName + ".acdata");
    const wstring acDataPathW = CEngineString::StringToWString(acDataPath);

    CAnimatorController::AnimatorControllerInitInfo acInfo = resources.ReadAnimatorControllerBufferInfos(acDataPathW);
    CAnimatorController* controller = CResources::LoadResourceComplete_Scene<CAnimatorController>(controllerNameW + L" (Animator Controller)", acDataPathW, nullptr, true);
    if (!controller)
        return nullptr;

    controller->Initiailize_Custom(acInfo);
    CResources::AddSceneResource(controllerNameW, controller, true);

    return controller;
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

    if ((!g_texturePickerState.material && !g_texturePickerState.image) || g_texturePickerState.slotIndex < 0)
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
                        if (g_texturePickerState.image)
                            g_texturePickerState.image->SetTexture(texture);
                        else
                            g_texturePickerState.material->Set_Texture(texture, g_texturePickerState.slotIndex);

                        g_texturePickerState.open = false;
                        g_texturePickerState.image = nullptr;
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

static _bool EnsureSceneEntryWithFormat(const fs::path& scenePath, const string& assetRelPath, const string& format, string& outEntryName)
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

    out << outEntryName << " : " << normalizedTarget << " : " << format << "\n";
    return true;
}

static _bool EnsureSceneMeshEntry(const fs::path& scenePath, const string& assetRelPath, string& outEntryName)
{
    return EnsureSceneEntryWithFormat(scenePath, assetRelPath, "[Mesh]", outEntryName);
}

static _bool EnsureSceneSkinnedMeshEntry(const fs::path& scenePath, const string& assetRelPath, string& outEntryName)
{
    return EnsureSceneEntryWithFormat(scenePath, assetRelPath, "[Skinned Mesh]", outEntryName);
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

static vector<SkinnedMeshBundle> EnsureSceneSkinnedMeshBundles(const wstring& sceneMeshResourceName, const string& skinnedDataFile)
{
    vector<SkinnedMeshBundle> bundles = CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(sceneMeshResourceName);
    if (!bundles.empty())
        return bundles;

    auto skinnedInfos = CResources::GetInstance().ReadSkinnedBufferInfos(CEngineString::StringToWString(skinnedDataFile));
    if (skinnedInfos.initList.empty())
        return {};

    return CResources::GetInstance().CreateSceneSkinnedBundle(sceneMeshResourceName, skinnedInfos.initList, skinnedInfos.skeletalList, FILTER_MESHBUFFER | FILTER_MATERIAL, nullptr, false);
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

static void ApplySkinnedMeshSelectionToObject(CGameObject* obj, CSkinnedMeshRenderer* skinnedMeshRenderer, const string& relPath, const _bool selectedSkinnedData)
{
    if (!obj || !skinnedMeshRenderer)
        return;

    CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
    if (!scene)
        return;

    const string normalizedRelPath = NormalizeSlashPath(relPath);
    string sceneEntryName = selectedSkinnedData ? fs::path(normalizedRelPath).stem().string() : string();

    if (!selectedSkinnedData)
    {
        const string ext = CEditor::ToLowerCopy(fs::path(normalizedRelPath).extension().string());
        const fs::path assetPath = fs::path("../Assets") / fs::path(normalizedRelPath);
        if (ext != ".fbx" || !fs::exists(assetPath))
            return;

        const fs::path scenePath = fs::path("../Assets/Scenes") / (CEngineString::WStringToString(scene->Get_SceneName()) + ".scene");
        if (!EnsureSceneSkinnedMeshEntry(scenePath, normalizedRelPath, sceneEntryName))
            return;
    }

    const string expectedSkinnedDataFile = sceneEntryName + ".skinneddata";
    const fs::path expectedSkinnedDataPath = fs::path("BinaryAssets/SkinnedMeshData") / expectedSkinnedDataFile;
    const _bool skinnedDataExistsInitially = fs::exists(expectedSkinnedDataPath);
    const wstring sceneMeshResourceName = CEngineString::StringToWString(sceneEntryName + " (MeshBuffer)");

    if (skinnedDataExistsInitially)
    {
        vector<SkinnedMeshBundle> bundles = EnsureSceneSkinnedMeshBundles(sceneMeshResourceName, expectedSkinnedDataFile);
        if (bundles.empty() || !bundles[0].meshBuffer)
            return;

        skinnedMeshRenderer->Set_MeshBuffer(bundles[0].meshBuffer);
        if (bundles[0].material)
            skinnedMeshRenderer->Set_Material(bundles[0].material);
        return;
    }

    if (selectedSkinnedData)
        return;

    CResources::GetInstance().ConvertFBXToSkinnedBufferData(CEngineString::StringToWString(normalizedRelPath));

    const string convertedBase = BuildMeshDataBaseName(normalizedRelPath);
    const fs::path convertedSkinnedDataPath = fs::path("BinaryAssets/SkinnedMeshData") / (convertedBase + ".skinneddata");
    if (!fs::exists(convertedSkinnedDataPath))
        return;

    if (convertedSkinnedDataPath != expectedSkinnedDataPath)
    {
        error_code ec;
        fs::copy_file(convertedSkinnedDataPath, expectedSkinnedDataPath, fs::copy_options::overwrite_existing, ec);
        if (ec)
            return;
    }

    vector<SkinnedMeshBundle> bundles = EnsureSceneSkinnedMeshBundles(sceneMeshResourceName, expectedSkinnedDataFile);
    if (bundles.empty() || !bundles[0].meshBuffer)
        return;

    skinnedMeshRenderer->Set_MeshBuffer(bundles[0].meshBuffer);
    if (bundles[0].material)
        skinnedMeshRenderer->Set_Material(bundles[0].material);
}

static void QueueSkinnedMeshSelectionRequest(CGameObject* obj, CSkinnedMeshRenderer* skinnedMeshRenderer, const string& relPath, const _bool selectedSkinnedData)
{
    if (!obj || !skinnedMeshRenderer)
        return;

    g_pendingSkinnedMeshSelectionRequest.obj = obj;
    g_pendingSkinnedMeshSelectionRequest.skinnedMeshRenderer = skinnedMeshRenderer;
    g_pendingSkinnedMeshSelectionRequest.relPath = relPath;
    g_pendingSkinnedMeshSelectionRequest.selectedSkinnedData = selectedSkinnedData;
    g_pendingSkinnedMeshSelectionRequest.pending = true;
}

static void ProcessPendingSkinnedMeshSelectionRequest()
{
    if (!g_pendingSkinnedMeshSelectionRequest.pending)
        return;

    PendingSkinnedMeshSelectionRequest req = g_pendingSkinnedMeshSelectionRequest;
    g_pendingSkinnedMeshSelectionRequest = {};

    if (!req.obj || !req.skinnedMeshRenderer)
        return;

    ApplySkinnedMeshSelectionToObject(req.obj, req.skinnedMeshRenderer, req.relPath, req.selectedSkinnedData);
}

static void RenderSkinnedPathTreeRecursive(const PathTreeNode& node, const string& idPrefix, CGameObject* obj, CSkinnedMeshRenderer* skinnedMeshRenderer, const _bool selectedSkinnedData)
{
    for (const auto& childPair : node.children)
    {
        const string& name = childPair.first;
        const PathTreeNode& child = childPair.second;

        if (child.children.empty() && child.isFile)
        {
            const string label = name + "##" + idPrefix + child.fullPath;
            if (ImGui::Selectable(label.c_str(), false))
                QueueSkinnedMeshSelectionRequest(obj, skinnedMeshRenderer, child.fullPath, selectedSkinnedData);
            continue;
        }

        const string nodeKey = child.fullPath.empty() ? name : child.fullPath;
        const string label = name + "##" + idPrefix + nodeKey;
        if (ImGui::TreeNode(label.c_str()))
        {
            RenderSkinnedPathTreeRecursive(child, idPrefix, obj, skinnedMeshRenderer, selectedSkinnedData);
            ImGui::TreePop();
        }
    }
}

static void RenderSkinnedPathTreeList(const vector<string>& files, const string& idPrefix, CGameObject* obj, CSkinnedMeshRenderer* skinnedMeshRenderer, const _bool selectedSkinnedData, const string& emptyText)
{
    if (files.empty())
    {
        ImGui::Selectable(emptyText.c_str(), false, ImGuiSelectableFlags_Disabled);
        return;
    }

    PathTreeNode root;
    for (const string& relPath : files)
        AddPathToTree(root, relPath);

    RenderSkinnedPathTreeRecursive(root, idPrefix, obj, skinnedMeshRenderer, selectedSkinnedData);
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

        static CGameObject* s_TagTarget = nullptr;
        static string s_NewTagName;
        if (s_TagTarget != selectedObj)
        {
            s_TagTarget = selectedObj;
            s_NewTagName.clear();
        }

        string selectedTagName = CEngineString::WStringToString(selectedObj->GetTag());
        if (selectedTagName.empty())
            selectedTagName = "Untagged";

        ImGui::Text("Tag");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        const string tagComboId = "##ObjTag" + to_string(selectedObj->Get_UniqueID());
        const string addTagPopupId = "AddTagPopup##" + to_string(selectedObj->Get_UniqueID());
        _bool openAddTagPopup = false;
        if (ImGui::BeginCombo(tagComboId.c_str(), selectedTagName.c_str()))
        {
            const auto& tagList = CSceneManager::GetInstance().Get_TagList();
            for (const wstring& tag : tagList)
            {
                const string tagName = CEngineString::WStringToString(tag);
                const _bool isSelected = selectedObj->GetTag() == tag;
                if (ImGui::Selectable(tagName.c_str(), isSelected))
                    selectedObj->SetTag(tag);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            if (ImGui::Selectable("Add Tag..."))
            {
                s_NewTagName.clear();
                openAddTagPopup = true;
            }

            ImGui::EndCombo();
        }

        if (openAddTagPopup)
            ImGui::OpenPopup(addTagPopupId.c_str());

        if (ImGui::BeginPopupModal(addTagPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetNextItemWidth(220.f);
            ImGui::InputText("Tag Name", &s_NewTagName, ImGuiInputTextFlags_AutoSelectAll);

            if (ImGui::Button("Add"))
            {
                const string trimmedTagName = CEngineString::Trim(s_NewTagName);
                if (!trimmedTagName.empty())
                {
                    const wstring newTag = CEngineString::StringToWString(trimmedTagName);
                    CSceneManager::GetInstance().AddTag(newTag);
                    selectedObj->SetTag(newTag);
                    s_NewTagName.clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Registered Tags");

            wstring removeTargetTag = L"";
            const auto& tagList = CSceneManager::GetInstance().Get_TagList();
            for (const wstring& tag : tagList)
            {
                const string tagName = CEngineString::WStringToString(tag);
                ImGui::TextUnformatted(tagName.c_str());

                if (tag != L"Untagged")
                {
                    ImGui::SameLine(220.f);
                    ImGui::PushID(tagName.c_str());
                    if (ImGui::Button("Remove"))
                        removeTargetTag = tag;
                    ImGui::PopID();
                }
            }

            if (!removeTargetTag.empty())
            {
                CSceneManager::GetInstance().RemoveTag(removeTargetTag);
                if (selectedObj->GetTag() == removeTargetTag)
                    selectedObj->SetTag(L"Untagged");
            }

            ImGui::EndPopup();
        }

        Toggle_End();

        struct StaticOption
        {
            const char* label;
            CGameObject::STATIC_METHOD method;
        };

        StaticOption staticOptions[] =
        {
            { "TransformStatic", CGameObject::STATIC_METHOD::TransformStatic },
            { "NavigationStatic", CGameObject::STATIC_METHOD::NavigationStatic }
        };

        string staticPreview = "None";
        _uint selectedStaticCount = 0;

        for (const auto& option : staticOptions)
        {
            if (selectedObj->IsStatic(option.method))
            {
                if (selectedStaticCount == 0)
                    staticPreview = option.label;
                else
                    staticPreview += ", " + string(option.label);

                ++selectedStaticCount;
            }
        }

        if (selectedStaticCount > 1)
            staticPreview = to_string(selectedStaticCount) + " Selected";

        static CGameObject* pendingStaticTarget = nullptr;
        static CGameObject::STATIC_METHOD pendingStaticMethod = CGameObject::STATIC_METHOD::TransformStatic;
        static _bool pendingStaticValue = false;
        static _bool pendingOpenStaticPopup = false;

        string staticPopupId = "Apply Static To Children?##" + to_string(selectedObj->Get_UniqueID());
        string staticComboId = "Static##" + to_string(selectedObj->Get_UniqueID());
        if (ImGui::BeginCombo(staticComboId.c_str(), staticPreview.c_str()))
        {
            for (const auto& option : staticOptions)
            {
                _bool enabled = selectedObj->IsStatic(option.method);
                string optionId = string(option.label) + "##" + to_string(selectedObj->Get_UniqueID());
                if (ImGui::Checkbox(optionId.c_str(), &enabled))
                {
                    CTransform* transform = selectedObj->GetTransform();
                    const _bool hasChildren = transform && !transform->Get_ChldList().empty();

                    if (hasChildren)
                    {
                        pendingStaticTarget = selectedObj;
                        pendingStaticMethod = option.method;
                        pendingStaticValue = enabled;
                        pendingOpenStaticPopup = true;
                    }
                    else
                        selectedObj->SetStatic(option.method, enabled, false);
                }
            }

            ImGui::EndCombo();
        }

        if (pendingOpenStaticPopup)
        {
            ImGui::OpenPopup(staticPopupId.c_str());
            pendingOpenStaticPopup = false;
        }

        if (ImGui::BeginPopupModal(staticPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Apply to children too?");

            if (ImGui::Button("Yes"))
            {
                if (pendingStaticTarget)
                {
                    pendingStaticTarget->SetStatic(pendingStaticMethod, pendingStaticValue, true);
                    pendingStaticTarget = nullptr;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("No"))
            {
                if (pendingStaticTarget)
                {
                    pendingStaticTarget->SetStatic(pendingStaticMethod, pendingStaticValue, false);
                    pendingStaticTarget = nullptr;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        CSceneManager& sceneManager = CSceneManager::GetInstance();
        const auto& layerList = sceneManager.Get_LayerList();
        _uint selectedLayerMask = selectedObj->GetLayer();

        string selectedLayerName = CEngineString::WStringToString(sceneManager.LayerToName(selectedLayerMask));
        if (selectedLayerName.empty())
            selectedLayerName = selectedLayerMask == 0u ? "Default" : "No Layer";

        const string layerComboId = "Layer##" + to_string(selectedObj->Get_UniqueID());
        if (ImGui::BeginCombo(layerComboId.c_str(), selectedLayerName.c_str()))
        {
            for (_uint i = 0u; i < 32u; ++i)
            {
                const _uint layerMask = (i == 0u) ? 0u : (1u << i);
                auto it = layerList.find(layerMask);
                if (it == layerList.end())
                    continue;

                const string layerName = CEngineString::WStringToString(it->second);
                if (layerMask != 0u && CEngineString::Trim(layerName).empty())
                    continue;

                const string optionLabel = to_string(i) + ": " + layerName;
                const bool isSelected = (selectedLayerMask == layerMask);
                if (ImGui::Selectable(optionLabel.c_str(), isSelected))
                    selectedObj->SetLayer(layerMask);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        const string addLayerPopupId = "AddLayerPopup##" + to_string(selectedObj->Get_UniqueID());
        static _int selectedLayerIndexForEdit = 1;
        static string editLayerName = "";

        if (ImGui::Button("AddLayer"))
        {
            _uint currentLayerMask = (_uint)1u << selectedLayerIndexForEdit;
            auto it = layerList.find(currentLayerMask);
            if (it != layerList.end())
                editLayerName = CEngineString::WStringToString(it->second);
            else
                editLayerName.clear();

            ImGui::OpenPopup(addLayerPopupId.c_str());
        }

        if (ImGui::BeginPopupModal(addLayerPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetNextItemWidth(220.f);
            ImGui::SliderInt("Layer Index", &selectedLayerIndexForEdit, 1, 31);

            const _uint editingMask = (_uint)1u << selectedLayerIndexForEdit;
            string currentLayerName = CEngineString::WStringToString(sceneManager.LayerToName(editingMask));
            if (currentLayerName.empty())
                currentLayerName = "<Empty>";
            ImGui::Text("Current: %s", currentLayerName.c_str());

            ImGui::SetNextItemWidth(220.f);
            ImGui::InputText("Layer Name", &editLayerName, ImGuiInputTextFlags_AutoSelectAll);
            ImGui::TextUnformatted("Leave empty to clear this layer.");

            if (ImGui::Button("Apply"))
            {
                const string trimmedName = CEngineString::Trim(editLayerName);
                sceneManager.AddLayer((_uint)selectedLayerIndexForEdit, CEngineString::StringToWString(trimmedName));
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (!selectedObj)
            return;

        ImGui::Text(selectedObj->IsBoneTransform() ? "Bone" : "");

        if (!selectedObj->GetComponent<CRectTransform>())
            ShowTransform(selectedObj);
        else
            ShowRectTransform(selectedObj);

        ShowComponents(selectedObj);
        ProcessPendingMeshSelectionRequest();
        ProcessPendingSkinnedMeshSelectionRequest();
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
    CTransform* transform = _obj->GetTransform();

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
    CRectTransform* rectTransform = dynamic_cast<CRectTransform*>(_obj->GetTransform());

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

        vector2 anchoredSize = rectTransform->Get_AnchoredSize();
        if (ImGui::BeginTable("Anchored Size Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Size");
            ImGui::TableSetColumnIndex(1);

            ImGui::TextUnformatted("Width");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##AnchoredWidth", &anchoredSize.x, 0.f))
                rectTransform->Set_AnchoredSize(anchoredSize);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::TextUnformatted("Height");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##AnchoredHeight", &anchoredSize.y, 0.f))
                rectTransform->Set_AnchoredSize(anchoredSize);
            ImGui::PopItemWidth();

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

        _float3 scale = rectTransform->Get_SizeScale();
        if (ImGui::BeginTable("Scale Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::TableSetColumnIndex(1);

            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##ScaleX", &scale.x, 0.f))
                rectTransform->Set_SizeScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##ScaleY", &scale.y, 0.f))
                rectTransform->Set_SizeScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##ScaleZ", &scale.z, 0.f))
                rectTransform->Set_SizeScale(scale);
            ImGui::PopItemWidth();

            ImGui::EndTable();
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
    for (auto it = components.begin(); it != components.end();)
    {
        CComponent* component = *it;
        ++it;

        if (!component)
            continue;

        if (dynamic_cast<CTransform*>(component) || dynamic_cast<CRectTransform*>(component))
            continue;

        string componentName = CEngineString::WStringToString(component->Get_UName());
        if (componentName.empty())
            continue;

        ImGui::PushID(component);
        const ImGuiTreeNodeFlags headerFlags =
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_AllowItemOverlap;
        const _bool headerOpen = ImGui::TreeNodeEx("ComponentHeader", headerFlags, "");
        const ImVec2 nextCursor = ImGui::GetCursorScreenPos();
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const ImGuiStyle& style = ImGui::GetStyle();
        const _float smallFramePaddingX = max(1.f, style.FramePadding.x - 1.f);
        const _float smallFramePaddingY = max(1.f, style.FramePadding.y - 1.f);
        const _float arrowWidth = ImGui::GetFontSize() + style.FramePadding.x * 2.f;
        const _float checkboxSize = ImGui::GetFontSize() + smallFramePaddingY * 2.f;
        const _float checkboxX = headerMin.x + arrowWidth;
        const _float checkboxY = headerMin.y + max(0.f, ((headerMax.y - headerMin.y) - checkboxSize) * 0.5f - 1.f);

        ImGui::SetCursorScreenPos(ImVec2(checkboxX, checkboxY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(smallFramePaddingX, smallFramePaddingY));
        _bool componentEnabled = component->Get_Enable();
        if (ImGui::Checkbox("##ComponentEnabled", &componentEnabled))
            component->SetEnable(componentEnabled);
        ImGui::PopStyleVar();

        ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(componentName.c_str());
        ImGui::SetCursorScreenPos(nextCursor);
        _bool removeComponent = false;

        if (headerOpen)
        {
            const string removeButtonLabel = "Remove Component##" + to_string(reinterpret_cast<uintptr_t>(component));
            if (ImGui::Button(removeButtonLabel.c_str()))
                removeComponent = true;

            if (!removeComponent)
            {

            if (CCamera* camera = dynamic_cast<CCamera*>(component))
            {
                static const char* clearFlagLabels[] = { "Skybox", "SolidColor", "DepthOnly", "DontClear" };
                static const char* viewModeLabels[] = { "Perspective", "Orthographic" };

                CSceneManager& sceneManager = CSceneManager::GetInstance();
                const auto& layerList = sceneManager.Get_LayerList();

                _int clearFlagIndex = static_cast<_int>(camera->GetClearFlags());
                if (ImGui::Combo(("Clear Flags##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), &clearFlagIndex, clearFlagLabels, IM_ARRAYSIZE(clearFlagLabels)))
                    camera->SetClearFlags(static_cast<CCamera::ClearFlags>(clearFlagIndex));

                _int viewModeIndex = static_cast<_int>(camera->GetViewMode());
                if (ImGui::Combo(("View Mode##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), &viewModeIndex, viewModeLabels, IM_ARRAYSIZE(viewModeLabels)))
                    camera->SetViewMode(static_cast<CCamera::ViewMode>(viewModeIndex));

                _float nearValue = camera->GetNear();
                if (ImGui::InputFloat(("Near##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), &nearValue, 0.01f, 0.1f, "%.3f"))
                    camera->SetNear(nearValue);

                _float farValue = camera->GetFar();
                if (ImGui::InputFloat(("Far##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), &farValue, 1.f, 10.f, "%.3f"))
                    camera->SetFar(farValue);

                if (camera->GetViewMode() == CCamera::ViewMode::Perspective)
                {
                    _float fovValue = camera->GetFieldOfView();
                    const string fovSliderLabel = "Field Of View##Slider" + to_string(reinterpret_cast<uintptr_t>(camera));
                    const string fovInputLabel = "FOV Input##" + to_string(reinterpret_cast<uintptr_t>(camera));
                    ImGui::Text("Field Of View");
                    ImGui::SetNextItemWidth(220.f);
                    _bool changed = ImGui::SliderFloat(("##" + fovSliderLabel).c_str(), &fovValue, 1.f, 179.f, "%.1f deg");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.f);
                    changed |= ImGui::InputFloat(fovInputLabel.c_str(), &fovValue, 1.f, 5.f, "%.1f");
                    if (changed)
                        camera->SetFieldOfView(fovValue);
                }
                else
                {
                    _float orthoSize = camera->GetOrthographicSize();
                    if (ImGui::InputFloat(("Orthographic Size##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), &orthoSize, 0.1f, 1.f, "%.3f"))
                        camera->SetOrthographicSize(orthoSize);
                }

                const _float aspectValue = camera->GetAspect();
                ImGui::Text("Aspect: %.4f", aspectValue);

                ColorValue bgColor = camera->Get_BackgroundColor();
                _float color[4] = { bgColor.r / 255.f, bgColor.g / 255.f, bgColor.b / 255.f, bgColor.a / 255.f };
                if (ImGui::ColorEdit4(("Background Color##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str(), color))
                {
                    camera->SetBackgroundColor(ColorValue(
                        static_cast<BYTE>(std::clamp(color[0], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(color[1], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(color[2], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(color[3], 0.f, 1.f) * 255.f)));
                }

                _uint cullingMask = camera->GetCullingMask();
                vector<string> selectedLayers;
                for (_uint i = 0u; i < 32u; ++i)
                {
                    const _uint layerValue = (i == 0u) ? 0u : (1u << i);
                    auto layerIt = layerList.find(layerValue);
                    if (layerIt == layerList.end())
                        continue;

                    if (sceneManager.ContainLayerMask(layerValue, cullingMask))
                        selectedLayers.push_back(CEngineString::WStringToString(layerIt->second));
                }

                string cullingPreview = "None";
                if (cullingMask == ~0u)
                    cullingPreview = "Everything";
                else if (!selectedLayers.empty())
                {
                    cullingPreview.clear();
                    const size_t previewCount = min<size_t>(selectedLayers.size(), 3u);
                    for (size_t i = 0; i < previewCount; ++i)
                    {
                        if (!cullingPreview.empty())
                            cullingPreview += ", ";
                        cullingPreview += selectedLayers[i];
                    }
                    if (selectedLayers.size() > previewCount)
                        cullingPreview += " ...";
                }

                ImGui::Separator();
                const string cullingComboLabel = "Culling Mask##" + to_string(reinterpret_cast<uintptr_t>(camera));
                if (ImGui::BeginCombo(cullingComboLabel.c_str(), cullingPreview.c_str()))
                {
                    if (ImGui::Selectable("Everything", cullingMask == ~0u))
                        cullingMask = ~0u;

                    if (ImGui::Selectable("Nothing", cullingMask == 0u))
                        cullingMask = 0u;

                    ImGui::Separator();

                    for (_uint i = 0u; i < 32u; ++i)
                    {
                        const _uint layerValue = (i == 0u) ? 0u : (1u << i);
                        auto layerIt = layerList.find(layerValue);
                        if (layerIt == layerList.end())
                            continue;

                        _bool enabled = sceneManager.ContainLayerMask(layerValue, cullingMask);
                        const string layerLabel = to_string(i) + ": " + CEngineString::WStringToString(layerIt->second) + "##CameraLayer" + to_string(reinterpret_cast<uintptr_t>(camera)) + "_" + to_string(i);
                        if (ImGui::Checkbox(layerLabel.c_str(), &enabled))
                        {
                            const _uint layerBit = (layerValue == 0u) ? 1u : layerValue;
                            if (enabled)
                                cullingMask |= layerBit;
                            else
                                cullingMask &= ~layerBit;
                        }
                    }

                    ImGui::EndCombo();
                }
                camera->SetCullingMask(cullingMask);

                if (ImGui::TreeNode(("Matrix Preview##" + to_string(reinterpret_cast<uintptr_t>(camera))).c_str()))
                {
                    _float4x4 viewMatrix = {};
                    _float4x4 projMatrix = {};
                    XMStoreFloat4x4(&viewMatrix, camera->GetViewMatrix());
                    XMStoreFloat4x4(&projMatrix, camera->GetProjectionMatrix());

                    const _float viewRows[4][4] =
                    {
                        { viewMatrix._11, viewMatrix._12, viewMatrix._13, viewMatrix._14 },
                        { viewMatrix._21, viewMatrix._22, viewMatrix._23, viewMatrix._24 },
                        { viewMatrix._31, viewMatrix._32, viewMatrix._33, viewMatrix._34 },
                        { viewMatrix._41, viewMatrix._42, viewMatrix._43, viewMatrix._44 }
                    };

                    const _float projRows[4][4] =
                    {
                        { projMatrix._11, projMatrix._12, projMatrix._13, projMatrix._14 },
                        { projMatrix._21, projMatrix._22, projMatrix._23, projMatrix._24 },
                        { projMatrix._31, projMatrix._32, projMatrix._33, projMatrix._34 },
                        { projMatrix._41, projMatrix._42, projMatrix._43, projMatrix._44 }
                    };

                    ImGui::TextUnformatted("View Matrix");
                    for (_int row = 0; row < 4; ++row)
                        ImGui::Text("[%.3f %.3f %.3f %.3f]", viewRows[row][0], viewRows[row][1], viewRows[row][2], viewRows[row][3]);

                    ImGui::Separator();
                    ImGui::TextUnformatted("Projection Matrix");
                    for (_int row = 0; row < 4; ++row)
                        ImGui::Text("[%.3f %.3f %.3f %.3f]", projRows[row][0], projRows[row][1], projRows[row][2], projRows[row][3]);

                    ImGui::TreePop();
                }
            }

            if (CLight* light = dynamic_cast<CLight*>(component))
            {
                static const char* lightTypeLabels[] = { "Directional", "Point", "Spot" };

                const string lightId = to_string(reinterpret_cast<uintptr_t>(light));
                _int lightTypeIndex = static_cast<_int>(light->Get_Type());
                if (ImGui::Combo(("Type##" + lightId).c_str(), &lightTypeIndex, lightTypeLabels, IM_ARRAYSIZE(lightTypeLabels)))
                    light->Set_Type(static_cast<CLight::Type>(lightTypeIndex));

                _float intensity = light->Get_Intensity();
                if (ImGui::DragFloat(("Intensity##" + lightId).c_str(), &intensity, 0.05f, 0.f, 100.f, "%.2f"))
                    light->Set_Intensity(intensity);

                if (light->Get_Type() != CLight::Type::Directional)
                {
                    _float range = light->Get_Range();
                    if (ImGui::DragFloat(("Range##" + lightId).c_str(), &range, 0.1f, 0.f, 10000.f, "%.2f"))
                        light->Set_Range(range);

                    _float attenuation = light->Get_Attenuation();
                    if (ImGui::DragFloat(("Attenuation##" + lightId).c_str(), &attenuation, 0.01f, 0.f, 100.f, "%.3f"))
                        light->Set_Attenuation(attenuation);
                }

                if (light->Get_Type() == CLight::Type::spot)
                {
                    _float spotAngle = light->Get_SpotAngle();
                    if (ImGui::SliderFloat(("Spot Angle##" + lightId).c_str(), &spotAngle, 1.f, 179.f, "%.1f deg"))
                        light->Set_SpotAngle(spotAngle);
                }

                ColorValue diffuseColor = light->Get_DiffuseColor();
                _float diffuse[4] = { diffuseColor.r / 255.f, diffuseColor.g / 255.f, diffuseColor.b / 255.f, diffuseColor.a / 255.f };
                if (ImGui::ColorEdit4(("Diffuse Color##" + lightId).c_str(), diffuse))
                {
                    light->Set_Color(ColorValue(
                        static_cast<BYTE>(std::clamp(diffuse[0], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(diffuse[1], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(diffuse[2], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(diffuse[3], 0.f, 1.f) * 255.f)));
                }

                ColorValue specularColor = light->Get_SpecularColor();
                _float specular[4] = { specularColor.r / 255.f, specularColor.g / 255.f, specularColor.b / 255.f, specularColor.a / 255.f };
                if (ImGui::ColorEdit4(("Specular Color##" + lightId).c_str(), specular))
                {
                    light->Set_SpecularColor(ColorValue(
                        static_cast<BYTE>(std::clamp(specular[0], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(specular[1], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(specular[2], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(specular[3], 0.f, 1.f) * 255.f)));
                }

                _bool castShadow = light->IsCastShadow();
                if (ImGui::Checkbox(("Cast Shadow##" + lightId).c_str(), &castShadow))
                    light->SetCastShadow(castShadow);
            }
            if (CMeshRenderer* meshRenderer = dynamic_cast<CMeshRenderer*>(component))
                RenderMeshRendererComponent(meshRenderer);

            if (CSkinnedMeshRenderer* skinnedMeshRenderer = dynamic_cast<CSkinnedMeshRenderer*>(component))
                RenderSkinnedMeshRendererComponent(_obj, skinnedMeshRenderer);

            if (CMeshFilter* meshFilter = dynamic_cast<CMeshFilter*>(component))
                RenderMeshFilterComponent(_obj, meshFilter);

            if (CAnimator* animator = dynamic_cast<CAnimator*>(component))
                RenderAnimatorComponent(_obj, animator);

            if (CCloth* cloth = dynamic_cast<CCloth*>(component))
            {
                _bool useGravity = cloth->GetUseGravity();
                if (ImGui::Checkbox("Use Gravity", &useGravity))
                    cloth->SetUseGravity(useGravity);
            }

            if (CNaviMeshAgent* navMeshAgent = dynamic_cast<CNaviMeshAgent*>(component))
            {
                const string navId = to_string(reinterpret_cast<uintptr_t>(navMeshAgent));

                string navigationResourceName = CEngineString::WStringToString(navMeshAgent->GetNavigationMeshResourceName());
                if (ImGui::InputText(("Navigation Mesh##" + navId).c_str(), &navigationResourceName))
                    navMeshAgent->SetNavigationMeshResourceName(CEngineString::StringToWString(navigationResourceName));

                ImGui::SameLine();
                if (ImGui::Button(("Use Scene Default##" + navId).c_str()))
                    navMeshAgent->SetNavigationMeshResourceName(L"");

                if (navigationResourceName.empty())
                    ImGui::TextDisabled("Empty value uses the scene default NavigationMesh resource.");

                _float agentRadius = navMeshAgent->GetAgentRadius();
                if (ImGui::DragFloat(("Radius##" + navId).c_str(), &agentRadius, 0.01f, 0.01f, 100.f, "%.2f"))
                    navMeshAgent->SetAgentRadius(agentRadius);

                _float agentHeight = navMeshAgent->GetAgentHeight();
                if (ImGui::DragFloat(("Height##" + navId).c_str(), &agentHeight, 0.01f, 0.01f, 100.f, "%.2f"))
                    navMeshAgent->SetAgentHeight(agentHeight);

                vector3 agentCenter = navMeshAgent->GetAgentCenter();
                _float centerValues[3] = { agentCenter.x, agentCenter.y, agentCenter.z };
                if (ImGui::InputFloat3(("Center##" + navId).c_str(), centerValues))
                    navMeshAgent->SetAgentCenter(vector3(centerValues[0], centerValues[1], centerValues[2]));

                _float moveSpeed = navMeshAgent->GetMoveSpeed();
                if (ImGui::DragFloat(("Move Speed##" + navId).c_str(), &moveSpeed, 0.05f, 0.f, 100.f, "%.2f"))
                    navMeshAgent->SetMoveSpeed(moveSpeed);

                _float angularSpeed = navMeshAgent->GetAngularSpeed();
                if (ImGui::DragFloat(("Angular Speed##" + navId).c_str(), &angularSpeed, 1.f, 0.f, 1440.f, "%.1f"))
                    navMeshAgent->SetAngularSpeed(angularSpeed);

                _float stoppingDistance = navMeshAgent->GetStoppingDistance();
                if (ImGui::DragFloat(("Stopping Distance##" + navId).c_str(), &stoppingDistance, 0.01f, 0.f, 20.f, "%.2f"))
                    navMeshAgent->SetStoppingDistance(stoppingDistance);

                _bool alwaysLookAt = navMeshAgent->GetAlwaysLookAt();
                if (ImGui::Checkbox(("Always Look At##" + navId).c_str(), &alwaysLookAt))
                    navMeshAgent->SetAlwaysLookAt(alwaysLookAt);

                _float waypointTolerance = navMeshAgent->GetWaypointTolerance();
                if (ImGui::DragFloat(("Waypoint Tolerance##" + navId).c_str(), &waypointTolerance, 0.01f, 0.001f, 20.f, "%.3f"))
                    navMeshAgent->SetWaypointTolerance(waypointTolerance);

                _float groundSnapOffset = navMeshAgent->GetGroundSnapOffset();
                if (ImGui::DragFloat(("Ground Snap Offset##" + navId).c_str(), &groundSnapOffset, 0.001f, 0.f, 1.f, "%.3f"))
                    navMeshAgent->SetGroundSnapOffset(groundSnapOffset);

                vector3 destination = navMeshAgent->GetDestination();
                _float destinationValues[3] = { destination.x, destination.y, destination.z };
                if (ImGui::InputFloat3(("Destination##" + navId).c_str(), destinationValues))
                    navMeshAgent->SetDestination(vector3(destinationValues[0], destinationValues[1], destinationValues[2]));

                if (ImGui::Button(("Reset Path##" + navId).c_str()))
                    navMeshAgent->ResetPath();

                const vector3 resolvedDestination = navMeshAgent->GetResolvedDestination();
                ImGui::Separator();
                ImGui::TextUnformatted("Runtime");
                ImGui::Text("On Navigation: %s", navMeshAgent->IsOnNavigation() ? "True" : "False");
                ImGui::Text("Has Destination: %s", navMeshAgent->HasDestination() ? "True" : "False");
                ImGui::Text("Has Path: %s", navMeshAgent->HasPath() ? "True" : "False");
                ImGui::Text("Current Polygon: %d", navMeshAgent->GetCurrentPolygonIndex());
                ImGui::Text("Path Points: %d", navMeshAgent->GetPathPointCount());
                ImGui::Text("Resolved Destination: (%.2f, %.2f, %.2f)", resolvedDestination.x, resolvedDestination.y, resolvedDestination.z);
            }

            if (CText* text = dynamic_cast<CText*>(component))
            {
                static const char* horizontalLabels[] = { "Left", "Center", "Right", "Justified", "Flush" };
                static const char* verticalLabels[] = { "Top", "Middle", "Bottom" };

                const string textId = to_string(reinterpret_cast<uintptr_t>(text));

                if (CFont* font = text->GetFont())
                {
                    const string fontName = CEngineString::WStringToString(font->Get_ResourceName());
                    ImGui::Text("Font: %s", fontName.empty() ? "None" : fontName.c_str());
                }
                else
                {
                    ImGui::TextUnformatted("Font: None");
                }

                string content = CEngineString::WStringToString(text->GetText());
                if (ImGui::InputTextMultiline(("Text##" + textId).c_str(), &content, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6.f)))
                    text->SetText(content);

                _float fontSize = text->GetFontSize();
                if (ImGui::DragFloat(("Font Size##" + textId).c_str(), &fontSize, 0.1f, 0.1f, 1000.f, "%.2f"))
                    text->SetFontSize(fontSize);

                _int horizontalIndex = static_cast<_int>(text->GetAligmentHorizontal());
                if (ImGui::Combo(("Horizontal##" + textId).c_str(), &horizontalIndex, horizontalLabels, IM_ARRAYSIZE(horizontalLabels)))
                    text->SetAligmentHorizontal(static_cast<CText::TextAligmentHorizontal>(horizontalIndex));

                _int verticalIndex = static_cast<_int>(text->GetAligmentVertical());
                if (ImGui::Combo(("Vertical##" + textId).c_str(), &verticalIndex, verticalLabels, IM_ARRAYSIZE(verticalLabels)))
                    text->SetAligmentVertical(static_cast<CText::TexAligmentVertical>(verticalIndex));

                ColorValue colorValue = text->GetColor();
                _float textColor[4] =
                {
                    colorValue.r / 255.f,
                    colorValue.g / 255.f,
                    colorValue.b / 255.f,
                    colorValue.a / 255.f
                };

                if (ImGui::ColorEdit4(("Color##" + textId).c_str(), textColor))
                {
                    text->SetColor(ColorValue(
                        static_cast<BYTE>(std::clamp(textColor[0], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(textColor[1], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(textColor[2], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(textColor[3], 0.f, 1.f) * 255.f)));
                }
            }

            if (CImage* image = dynamic_cast<CImage*>(component))
            {
                static const char* fillMethodLabels[] =
                {
                    "None",
                    "Horizontal",
                    "Vertical",
                    "Radial90",
                    "Radial180",
                    "Radial360"
                };

                const string imageId = to_string(reinterpret_cast<uintptr_t>(image));
                CTexture* texture = image->GetTexture();
                const string textureName = texture ? CEngineString::WStringToString(texture->Get_ResourceName()) : "None";

                ImGui::TextUnformatted("Texture");
                ImGui::SameLine();

                _bool openPicker = false;
                if (texture && texture->Get_SRV())
                    openPicker = ImGui::ImageButton(("##ImageTextureThumb" + imageId).c_str(), ImTextureRef((ImTextureID)(intptr_t)texture->Get_SRV()), ImVec2(36.f, 36.f));
                else
                    openPicker = ImGui::Button(("Select##ImageTexture" + imageId).c_str(), ImVec2(64.f, 36.f));

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", textureName.c_str());

                if (openPicker)
                    OpenTexturePicker(image);

                ImGui::SameLine();
                ImGui::Text("%s", BuildShortLabel(textureName, 28).c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", textureName.c_str());

                ImGui::SameLine();
                if (ImGui::Button(("Empty##ImageTexture" + imageId).c_str()))
                    image->SetTexture(nullptr);

                ColorValue colorValue = image->GetColor();
                _float imageColor[4] =
                {
                    colorValue.r / 255.f,
                    colorValue.g / 255.f,
                    colorValue.b / 255.f,
                    colorValue.a / 255.f
                };

                if (ImGui::ColorEdit4(("Color##Image" + imageId).c_str(), imageColor))
                {
                    image->SetColor(ColorValue(
                        static_cast<BYTE>(std::clamp(imageColor[0], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(imageColor[1], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(imageColor[2], 0.f, 1.f) * 255.f),
                        static_cast<BYTE>(std::clamp(imageColor[3], 0.f, 1.f) * 255.f)));
                }

                _int fillMethodIndex = static_cast<_int>(image->Get_FillMethod());
                if (ImGui::Combo(("Fill Method##Image" + imageId).c_str(), &fillMethodIndex, fillMethodLabels, IM_ARRAYSIZE(fillMethodLabels)))
                    image->Set_FillMethod(static_cast<CImage::FillMethod>(fillMethodIndex));

                _float fillAmount = image->GetFillAmount();
                if (ImGui::SliderFloat(("Fill Amount##Image" + imageId).c_str(), &fillAmount, 0.f, 1.f, "%.3f"))
                    image->SetFillAmount(fillAmount);
            }

			if (CHorizontalLayoutGroup* horizontalLayout = dynamic_cast<CHorizontalLayoutGroup*>(component))
			{
				static const char* alignmentLabels[] =
				{
					"Upper Left", "Upper Center", "Upper Right",
					"Middle Left", "Middle Center", "Middle Right",
					"Lower Left", "Lower Center", "Lower Right"
				};

				const string layoutId = to_string(reinterpret_cast<uintptr_t>(horizontalLayout));
				CLayoutGroup::Padding padding = horizontalLayout->GetPadding();
				_float paddingValues[4] = { padding.left, padding.right, padding.top, padding.bottom };
				if (ImGui::InputFloat4(("Padding (L,R,T,B)##" + layoutId).c_str(), paddingValues))
				{
					horizontalLayout->SetPadding(paddingValues[0], paddingValues[1], paddingValues[2], paddingValues[3]);
				}

				_float spacing = horizontalLayout->GetSpacing();
				if (ImGui::DragFloat(("Spacing##" + layoutId).c_str(), &spacing, 0.1f, 0.f, 1000.f, "%.2f"))
					horizontalLayout->SetSpacing(spacing);

				_int alignmentIndex = static_cast<_int>(horizontalLayout->GetChildAlignment());
				if (ImGui::Combo(("Child Alignment##" + layoutId).c_str(), &alignmentIndex, alignmentLabels, IM_ARRAYSIZE(alignmentLabels)))
					horizontalLayout->SetChildAlignment(static_cast<CLayoutGroup::ChildAlignment>(alignmentIndex));

				_bool controlWidth = horizontalLayout->GetControlChildSizeWidth();
				_bool controlHeight = horizontalLayout->GetControlChildSizeHeight();
				_bool controlChanged = false;
				controlChanged |= ImGui::Checkbox(("Control Child Width##" + layoutId).c_str(), &controlWidth);
				controlChanged |= ImGui::Checkbox(("Control Child Height##" + layoutId).c_str(), &controlHeight);
				if (controlChanged)
				{
					horizontalLayout->SetControlChildSize(controlWidth, controlHeight);
				}

				_bool forceWidth = horizontalLayout->GetForceExpandWidth();
				_bool forceHeight = horizontalLayout->GetForceExpandHeight();
				_bool forceChanged = false;
				forceChanged |= ImGui::Checkbox(("Force Expand Width##" + layoutId).c_str(), &forceWidth);
				forceChanged |= ImGui::Checkbox(("Force Expand Height##" + layoutId).c_str(), &forceHeight);
				if (forceChanged)
				{
					horizontalLayout->SetForceExpand(forceWidth, forceHeight);
				}
			}

			if (CVerticalLayoutGroup* verticalLayout = dynamic_cast<CVerticalLayoutGroup*>(component))
			{
				static const char* alignmentLabels[] =
				{
					"Upper Left", "Upper Center", "Upper Right",
					"Middle Left", "Middle Center", "Middle Right",
					"Lower Left", "Lower Center", "Lower Right"
				};

				const string layoutId = to_string(reinterpret_cast<uintptr_t>(verticalLayout));
				CLayoutGroup::Padding padding = verticalLayout->GetPadding();
				_float paddingValues[4] = { padding.left, padding.right, padding.top, padding.bottom };
				if (ImGui::InputFloat4(("Padding (L,R,T,B)##" + layoutId).c_str(), paddingValues))
				{
					verticalLayout->SetPadding(paddingValues[0], paddingValues[1], paddingValues[2], paddingValues[3]);
				}

				_float spacing = verticalLayout->GetSpacing();
				if (ImGui::DragFloat(("Spacing##" + layoutId).c_str(), &spacing, 0.1f, 0.f, 1000.f, "%.2f"))
					verticalLayout->SetSpacing(spacing);

				_int alignmentIndex = static_cast<_int>(verticalLayout->GetChildAlignment());
				if (ImGui::Combo(("Child Alignment##" + layoutId).c_str(), &alignmentIndex, alignmentLabels, IM_ARRAYSIZE(alignmentLabels)))
					verticalLayout->SetChildAlignment(static_cast<CLayoutGroup::ChildAlignment>(alignmentIndex));

				_bool controlWidth = verticalLayout->GetControlChildSizeWidth();
				_bool controlHeight = verticalLayout->GetControlChildSizeHeight();
				_bool controlChanged = false;
				controlChanged |= ImGui::Checkbox(("Control Child Width##" + layoutId).c_str(), &controlWidth);
				controlChanged |= ImGui::Checkbox(("Control Child Height##" + layoutId).c_str(), &controlHeight);
				if (controlChanged)
				{
					verticalLayout->SetControlChildSize(controlWidth, controlHeight);
				}

				_bool forceWidth = verticalLayout->GetForceExpandWidth();
				_bool forceHeight = verticalLayout->GetForceExpandHeight();
				_bool forceChanged = false;
				forceChanged |= ImGui::Checkbox(("Force Expand Width##" + layoutId).c_str(), &forceWidth);
				forceChanged |= ImGui::Checkbox(("Force Expand Height##" + layoutId).c_str(), &forceHeight);
				if (forceChanged)
				{
					verticalLayout->SetForceExpand(forceWidth, forceHeight);
				}
			}

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

            if (CMeshCollider* meshCollider = dynamic_cast<CMeshCollider*>(component))
            {
                _bool isTrigger = meshCollider->IsTrigger();
                if (ImGui::Checkbox("Is Trigger", &isTrigger))
                    meshCollider->SetTrigger(isTrigger);

                _bool showGizmo = meshCollider->IsGizmoVisible();
                if (ImGui::Checkbox("Show Gizmo", &showGizmo))
                    meshCollider->SetGizmoVisible(showGizmo);

                vector3 center = meshCollider->GetCenter();
                _float centerValues[3] = { center.x, center.y, center.z };
                if (ImGui::InputFloat3("Center", centerValues))
                    meshCollider->SetCenter(vector3(centerValues[0], centerValues[1], centerValues[2]));

                ImGui::Separator();
                ImGui::TextUnformatted("Preview");
                ImGui::Text("Center: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
                ImGui::Text("Is Trigger: %s", meshCollider->IsTrigger() ? "True" : "False");
                ImGui::Text("Show Gizmo: %s", meshCollider->IsGizmoVisible() ? "True" : "False");
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
                _bool constPosY = rigidBody->IsConstPositionY();
                _bool constPosZ = rigidBody->IsConstPositionZ();

                ImGui::TextUnformatted("Freeze Location");
                ImGui::SameLine();
                if (ImGui::Checkbox("X##FreezePosX", &constPosX))
                    rigidBody->SetConstPositionX(constPosX);
                ImGui::SameLine();
                if (ImGui::Checkbox("Y##FreezePosY", &constPosY))
                    rigidBody->SetConstPositionY(constPosY);
                ImGui::SameLine();
                if (ImGui::Checkbox("Z##FreezePosZ", &constPosZ))
                    rigidBody->SetConstPositionZ(constPosZ);

                _bool constRotX = rigidBody->IsConstRotationX();
                _bool constRotY = rigidBody->IsConstRotationY();
                _bool constRotZ = rigidBody->IsConstRotationZ();

                ImGui::TextUnformatted("Freeze Rotation");
                ImGui::SameLine();
                if (ImGui::Checkbox("X##FreezeRotX", &constRotX))
                    rigidBody->SetConstRotationX(constRotX);
                ImGui::SameLine();
                if (ImGui::Checkbox("Y##FreezeRotY", &constRotY))
                    rigidBody->SetConstRotationY(constRotY);
                ImGui::SameLine();
                if (ImGui::Checkbox("Z##FreezeRotZ", &constRotZ))
                    rigidBody->SetConstRotationZ(constRotZ);
            }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        if (removeComponent)
        {
            _obj->RemoveComponent(component);
            break;
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

void CInspectorBox::RenderSkinnedMeshRendererComponent(CGameObject* _obj, CSkinnedMeshRenderer* _skinnedMeshRenderer)
{
    if (!_obj || !_skinnedMeshRenderer)
        return;

    CMaterial* material = _skinnedMeshRenderer->Get_Material();
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

        if (ImGui::TreeNode("Material Properties"))
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

    vector<pair<string, CSkinnedMeshBuffer*>> meshOptions;
    vector<string> fbxFiles = CollectRelativeFilesByExtension(fs::path(L"../Assets"), ".fbx");
    vector<string> skinnedDataFiles = CollectRelativeFilesByExtension(fs::path(L"BinaryAssets/SkinnedMeshData"), ".skinneddata");
    CResources& resources = CResources::GetInstance();

    for (auto& entry : resources.m_mGameResourceList)
    {
        CSkinnedMeshBuffer* meshBuffer = dynamic_cast<CSkinnedMeshBuffer*>(entry.second);
        if (!meshBuffer)
            continue;

        string resourceName = CEngineString::WStringToString(meshBuffer->Get_ResourceName());
        meshOptions.push_back({ resourceName, meshBuffer });
    }

    sort(meshOptions.begin(), meshOptions.end(), [](const auto& a, const auto& b)
    {
        return a.first < b.first;
    });

    CSkinnedMeshBuffer* currentMeshBuffer = _skinnedMeshRenderer->Get_SkinnedMeshBuffer();
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

    const string comboLabel = "SkinnedMeshBuffer##" + to_string(_obj->Get_UniqueID()) + "_" + to_string(reinterpret_cast<uintptr_t>(_skinnedMeshRenderer));

    if (ImGui::BeginCombo(comboLabel.c_str(), currentName.c_str()))
    {
        if (ImGui::Selectable("None", currentMeshBuffer == nullptr))
            _skinnedMeshRenderer->Set_MeshBuffer(nullptr);

        for (_uint i = 0; i < meshOptions.size(); ++i)
        {
            const bool selected = (static_cast<_int>(i) == currentIndex);
            if (ImGui::Selectable(meshOptions[i].first.c_str(), selected))
                _skinnedMeshRenderer->Set_MeshBuffer(meshOptions[i].second);

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::TreeNode("Assets .fbx (Skinned)"))
    {
        if (ImGui::BeginChild("##skinned_fbx_tree_box", ImVec2(0.f, 180.f), true))
            RenderSkinnedPathTreeList(fbxFiles, "skinned_fbx_tree_", _obj, _skinnedMeshRenderer, false, "(No .fbx files)");
        ImGui::EndChild();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("BinaryAssets .skinneddata"))
    {
        if (ImGui::BeginChild("##skinneddata_tree_box", ImVec2(0.f, 180.f), true))
            RenderSkinnedPathTreeList(skinnedDataFiles, "skinneddata_tree_", _obj, _skinnedMeshRenderer, true, "(No .skinneddata files)");
        ImGui::EndChild();
        ImGui::TreePop();
    }
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

void CInspectorBox::RenderAnimatorComponent(CGameObject* _obj, CAnimator* _animator)
{
    if (!_obj || !_animator)
        return;

    const string animatorId = to_string(reinterpret_cast<uintptr_t>(_animator));
    CAnimatorController* controller = _animator->Get_Controller();
    const string currentControllerName = controller ? NormalizeAnimatorControllerName(controller->Get_ResourceName()) : "None";
    const auto& controllerFiles = GetAnimatorControllerRelativeFiles();
    const string currentControllerRelPath = FindAnimatorControllerRelativePath(controller);

    if (ImGui::BeginCombo(("Controller##" + animatorId).c_str(), currentControllerName.c_str()))
    {
        if (ImGui::Selectable("None", controller == nullptr))
            _animator->Set_Controller(nullptr);

        for (const string& relPath : controllerFiles)
        {
            const string stem = fs::path(relPath).stem().string();
            const _bool selected = controller && stem == currentControllerName;
            if (ImGui::Selectable(relPath.c_str(), selected))
            {
                if (CAnimatorController* selectedController = LoadInspectorAnimatorControllerResource(relPath))
                    _animator->Set_Controller(selectedController);
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (!currentControllerRelPath.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button(("Open Controller##" + animatorId).c_str()))
            CEditor::GetInstance().OpenAnimatorController(fs::path(L"../Assets") / fs::path(currentControllerRelPath));
    }

    _bool applyRootMotion = _animator->ApplyRootmotion();
    if (ImGui::Checkbox(("Apply Root Motion##" + animatorId).c_str(), &applyRootMotion))
        _animator->SetApplyRootmotion(applyRootMotion, applyRootMotion ? _obj->GetTransform() : nullptr);

    _float playbackSpeed = _animator->Get_PlaybackSpeed();
    if (ImGui::InputFloat(("Playback Speed##" + animatorId).c_str(), &playbackSpeed, 0.1f, 1.f, "%.3f"))
        _animator->Set_PlaybackSpeed(max(0.01f, playbackSpeed));

    if (ImGui::Button(("Play##" + animatorId).c_str()))
        _animator->Play();
    ImGui::SameLine();
    if (ImGui::Button(("Pause##" + animatorId).c_str()))
        _animator->Pause();
    ImGui::SameLine();
    if (ImGui::Button(("Stop##" + animatorId).c_str()))
        _animator->Stop();

    const string currentState = CEngineString::WStringToString(_animator->Get_CurrentState());
    ImGui::Text("Current State: %s", currentState.empty() ? "None" : currentState.c_str());

    CAnimationClip* currentAnimation = _animator->Get_CurrentDisplayAnimation();
    const string currentAnimationName = currentAnimation ? CEngineString::WStringToString(currentAnimation->Get_ResourceName()) : "None";
    ImGui::Text("Current Clip: %s", currentAnimationName.c_str());
    ImGui::Text("Playing: %s", _animator->IsPlaying() ? "True" : "False");
    ImGui::Text("Loop: %s", _animator->IsLoop() ? "True" : "False");
    ImGui::Text("Normalized Time: %.3f", _animator->GetNormalizedTime());

    if (!controller)
        return;

    if (ImGui::TreeNode(("Parameters##" + animatorId).c_str()))
    {
        vector<const CAnimatorController::ParameterDesc*> parameters;
        parameters.reserve(controller->Get_ParamMap().size());

        for (const auto& entry : controller->Get_ParamMap())
            parameters.push_back(&entry.second);

        sort(parameters.begin(), parameters.end(), [](const auto* lhs, const auto* rhs)
            {
                return lhs->name < rhs->name;
            });

        for (const CAnimatorController::ParameterDesc* parameter : parameters)
        {
            if (!parameter)
                continue;

            const string paramLabel = CEngineString::WStringToString(parameter->name);
            switch (parameter->type)
            {
            case CAnimatorController::PARAM_TYPE::BOOL:
            {
                _bool value = parameter->defaultBool;
                _animator->GetBool(parameter->name, value);
                if (ImGui::Checkbox((paramLabel + "##Bool" + animatorId).c_str(), &value))
                    _animator->SetBool(parameter->name, value);
                break;
            }
            case CAnimatorController::PARAM_TYPE::INT:
            {
                _int value = parameter->defaultInt;
                _animator->GetInt(parameter->name, value);
                if (ImGui::InputInt((paramLabel + "##Int" + animatorId).c_str(), &value))
                    _animator->SetInt(parameter->name, value);
                break;
            }
            case CAnimatorController::PARAM_TYPE::FLOAT:
            {
                _float value = parameter->defaultFloat;
                _animator->GetFloat(parameter->name, value);
                if (ImGui::InputFloat((paramLabel + "##Float" + animatorId).c_str(), &value, 0.1f, 1.f, "%.3f"))
                    _animator->SetFloat(parameter->name, value);
                break;
            }
            case CAnimatorController::PARAM_TYPE::TRIGGER:
            {
                ImGui::TextUnformatted(paramLabel.c_str());
                ImGui::SameLine();
                if (ImGui::Button(("Fire##Trigger" + animatorId + paramLabel).c_str()))
                    _animator->SetTrigger(parameter->name);
                break;
            }
            default:
                break;
            }
        }

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

		if (ImGui::MenuItem("HorizontalLayoutGroup"))
		{
			if (!_obj->GetComponent<CHorizontalLayoutGroup>())
				_obj->AddComponent<CHorizontalLayoutGroup>();
		}

		if (ImGui::MenuItem("VerticalLayoutGroup"))
		{
			if (!_obj->GetComponent<CVerticalLayoutGroup>())
				_obj->AddComponent<CVerticalLayoutGroup>();
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

        if (ImGui::MenuItem("MeshCollider"))
        {
            if (!_obj->GetComponent<CMeshCollider>())
                _obj->AddComponent<CMeshCollider>();
        }

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("RigidBody"))
    {
        if (!_obj->GetComponent<CRigidBody>())
            _obj->AddComponent<CRigidBody>();
    }

    if (ImGui::MenuItem("NaviMeshAgent"))
    {
        if (!_obj->GetComponent<CNaviMeshAgent>())
            _obj->AddComponent<CNaviMeshAgent>();
    }

    if (ImGui::MenuItem("Cloth"))
    {
        if (!_obj->GetComponent<CCloth>())
            _obj->AddComponent<CCloth>();
    }

    ImGui::EndPopup();
}

static _bool IsPreviewImageExtension(const fs::path& path)
{
    string ext = CEditor::ToLowerCopy(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".dds" || ext == ".tif" || ext == ".tiff" || ext == ".gif";
}

static _bool TryGetAssetsRelativePath(const fs::path& path, wstring& outRel)
{
    const fs::path assetsRoot = fs::path(L"../Assets");
    const fs::path binaryRoot = fs::path(L"BinaryAssets");
    error_code ec;

    auto makeCanonical = [&](const fs::path& value)
    {
        fs::path result = fs::weakly_canonical(value, ec);
        if (ec)
        {
            ec.clear();
            result = fs::absolute(value, ec);
        }
        return result;
    };

    const fs::path absolutePath = makeCanonical(path);
    if (ec)
        return false;

    const fs::path absoluteAssetsRoot = makeCanonical(assetsRoot);
    if (!ec)
    {
        fs::path relative = absolutePath.lexically_relative(absoluteAssetsRoot);
        if (!relative.empty() && relative.native().find(L"..") != 0)
        {
            outRel = relative.generic_wstring();
            return true;
        }
    }

    ec.clear();
    const fs::path absoluteBinaryRoot = makeCanonical(binaryRoot);
    if (ec)
        return false;

    fs::path relativeBinary = absolutePath.lexically_relative(absoluteBinaryRoot);
    if (!relativeBinary.empty() && relativeBinary.native().find(L"..") != 0)
    {
        outRel = L"BinaryAssets/" + relativeBinary.generic_wstring();
        return true;
    }

    return false;
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



















