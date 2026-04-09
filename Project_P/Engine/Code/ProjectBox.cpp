#include "epch.h"
#include "ProjectBox.h"

static void ShowInExplorer(const fs::path& path, const _bool selectItem)
{
#ifdef _WIN32
	error_code ec;
	fs::path targetPath = fs::weakly_canonical(path, ec);
	if (ec)
		targetPath = fs::absolute(path, ec);
	if (ec)
		targetPath = path;

	if (selectItem && fs::is_regular_file(targetPath))
	{
		wstring args = L"/select,\"" + targetPath.wstring() + L"\"";
		HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
		if ((INT_PTR)result <= 32)
			CDebug::LogError(L"ShowInExplorer failed: " + targetPath.wstring());
		return;
	}

	HINSTANCE result = ShellExecuteW(nullptr, L"open", targetPath.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if ((INT_PTR)result <= 32)
		CDebug::LogError(L"ShowInExplorer failed: " + targetPath.wstring());
#else
	CDebug::LogError(L"ShowInExplorer is not implemented on this platform.");
#endif
}

static string ToLowerCopy(const string& s)
{
    string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return out;
}

static _bool ContainsCaseInsensitive(const string& haystack, const string& needle)
{
    if (needle.empty())
        return true;
    const string h = ToLowerCopy(haystack);
    const string n = ToLowerCopy(needle);
    return h.find(n) != string::npos;
}

static _bool DirectoryMatchesQuery(const fs::path& dir, const string& query)
{
    if (query.empty())
        return true;

    error_code ec;
    if (ContainsCaseInsensitive(dir.filename().string(), query))
        return true;

    for (const auto& entry : fs::recursive_directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (ContainsCaseInsensitive(entry.path().filename().string(), query))
            return true;
    }

    return false;
}

static string NormalizeSlashPath(const string& path)
{
    string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

template<typename ConvertFn>
static void BatchConvertFiles(const vector<string>& targets, const string& label, ConvertFn convertFn)
{
    const int total = (int)targets.size();
    if (total > 1)
        CDebug::Log("[Batch:" + label + "] Starting " + to_string(total) + " files");

    int succeeded = 0;
    for (int i = 0; i < total; ++i)
    {
        const string fname = fs::path(targets[i]).filename().string();
        if (total > 1)
            CDebug::Log("[" + to_string(i + 1) + "/" + to_string(total) + "] " + fname);

        if (SUCCEEDED(convertFn(targets[i])))
            ++succeeded;
        else
            CDebug::LogError("[" + to_string(i + 1) + "/" + to_string(total) + "] Failed: " + fname);
    }

    if (total > 1)
        CDebug::Log("[Batch:" + label + "] Done " + to_string(succeeded) + "/" + to_string(total));
}

template<typename ExtPred>
static vector<string> CollectFilesFromPaths(const vector<string>& paths, ExtPred extPred)
{
    vector<string> result;
    for (const string& p : paths)
    {
        error_code ec;
        if (fs::is_directory(p, ec))
        {
            for (const auto& e : fs::recursive_directory_iterator(p, ec))
            {
                if (e.is_regular_file())
                {
                    string ext = ToLowerCopy(e.path().extension().string());
                    if (!ext.empty()) ext = ext.substr(1);
                    if (extPred(ext))
                        result.push_back(e.path().string());
                }
            }
        }
        else if (fs::is_regular_file(p, ec))
        {
            string ext = ToLowerCopy(fs::path(p).extension().string());
            if (!ext.empty()) ext = ext.substr(1);
            if (extPred(ext))
                result.push_back(p);
        }
    }
    return result;
}

static void DeleteAssociatedBinariesForFile(const fs::path& filePath)
{
    const string ext = ToLowerCopy(filePath.extension().string());
    const string folder = filePath.parent_path().filename().string();
    const string stem = filePath.stem().string();
    const string binaryPrefix = folder + "_" + stem;

    struct BinaryMapping { string dir; string ext; };
    vector<BinaryMapping> mappings;

    if (ext == ".fbx")
    {
        mappings.push_back({ "BinaryAssets/MeshData", ".meshdata" });
        mappings.push_back({ "BinaryAssets/SkinnedMeshData", ".skinneddata" });
        mappings.push_back({ "BinaryAssets/AnimationClipData", ".animdata" });
    }
    else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".tif" || ext == ".tiff")
    {
        mappings.push_back({ "BinaryAssets/TextureData", ".dds" });
    }
    else if (ext == ".animatorcontroller")
    {
        mappings.push_back({ "BinaryAssets/AnimatorControllerData", ".acdata" });
    }
    else if (ext == ".ttf" || ext == ".otf")
    {
        mappings.push_back({ "BinaryAssets/FontData", ".spritefont" });
    }

    for (const auto& m : mappings)
    {
        fs::path binaryPath = fs::path(m.dir) / (binaryPrefix + m.ext);
        error_code ec;
        if (fs::exists(binaryPath, ec))
        {
            fs::remove(binaryPath, ec);
            if (!ec)
                CDebug::Log(L"Deleted associated binary: " + binaryPath.wstring());
        }
    }
}

static void DeleteAssociatedBinaries(const fs::path& sourcePath)
{
    error_code ec;
    if (fs::is_directory(sourcePath, ec))
    {
        for (const auto& entry : fs::recursive_directory_iterator(sourcePath, ec))
        {
            if (entry.is_regular_file())
                DeleteAssociatedBinariesForFile(entry.path());
        }
    }
    else if (fs::is_regular_file(sourcePath, ec))
    {
        DeleteAssociatedBinariesForFile(sourcePath);
    }
}

static string BuildAssetRelativePath(const fs::path& path)
{
    error_code ec;
    const fs::path relative = path.lexically_relative(fs::path(L"../Assets"));
    if (relative.empty() || (!relative.native().empty() && relative.native()[0] == L'.'))
        return {};

    return NormalizeSlashPath(relative.generic_string());
}

static string BuildSceneStoredResourcePath(const fs::path& path)
{
    const string assetRelativePath = BuildAssetRelativePath(path);
    if (!assetRelativePath.empty())
        return assetRelativePath;

    return NormalizeSlashPath(path.lexically_normal().generic_string());
}

static fs::path BuildSceneFilePath(const wstring& sceneName)
{
    return fs::path(L"../Assets/Scenes") / fs::path(sceneName + L".scene");
}

static _bool UpdateSceneResourcePathsOnDisk(const fs::path& sceneFilePath, const unordered_map<string, string>& replacements, _uint* outUpdatedCount = nullptr)
{
    if (outUpdatedCount)
        *outUpdatedCount = 0u;

    if (replacements.empty())
        return true;

    ifstream in(sceneFilePath);
    if (!in.is_open())
        return false;

    vector<string> lines;
    string line;
    _uint updatedCount = 0u;

    while (getline(in, line))
    {
        const size_t firstSeparator = line.find(" : ");
        const size_t secondSeparator = firstSeparator == string::npos ? string::npos : line.find(" : ", firstSeparator + 3u);

        if (firstSeparator != string::npos && secondSeparator != string::npos)
        {
            const string currentPath = NormalizeSlashPath(line.substr(firstSeparator + 3u, secondSeparator - (firstSeparator + 3u)));
            const auto found = replacements.find(ToLowerCopy(currentPath));
            if (found != replacements.end())
            {
                line = line.substr(0, firstSeparator + 3u) + found->second + line.substr(secondSeparator);
                ++updatedCount;
            }
        }

        lines.push_back(line);
    }

    in.close();

    if (updatedCount == 0u)
        return true;

    ofstream out(sceneFilePath, ios::trunc);
    if (!out.is_open())
        return false;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        out << lines[i];
        if (i + 1u < lines.size())
            out << "\n";
    }

    if (outUpdatedCount)
        *outUpdatedCount = updatedCount;

    return out.good();
}

static void RemapLoadedSceneResourcePaths(const vector<pair<fs::path, fs::path>>& filePairs)
{
    if (filePairs.empty())
        return;

    unordered_set<CScene*> remappedScenes;
    auto remapScene = [&](CScene* scene)
        {
            if (!scene || !remappedScenes.insert(scene).second)
                return;

            scene->RemapResourceFilePaths(filePairs);
        };

    for (const auto& [sceneName, scene] : CSceneManager::GetInstance().Get_SceneList())
        remapScene(scene);

    remapScene(CSceneManager::GetInstance().Get_CrtScene());
    remapScene(CSceneManager::GetInstance().Get_TempScene());
}

static void UpdateSceneReferencesForRenamePairs(const vector<pair<fs::path, fs::path>>& filePairs)
{
    if (filePairs.empty())
        return;

    unordered_map<wstring, unordered_map<string, string>> sceneReplacements;

    for (const auto& filePair : filePairs)
    {
        const string oldScenePath = BuildSceneStoredResourcePath(filePair.first);
        const string newScenePath = BuildSceneStoredResourcePath(filePair.second);
        if (oldScenePath.empty() || newScenePath.empty())
            continue;

        const vector<wstring> usageScenes = CResources::GetInstance().GetResourceUsageScenes(filePair.first);
        for (const wstring& sceneName : usageScenes)
            sceneReplacements[sceneName][ToLowerCopy(oldScenePath)] = newScenePath;
    }

    for (const auto& [sceneName, replacements] : sceneReplacements)
    {
        _uint updatedCount = 0u;
        const fs::path sceneFilePath = BuildSceneFilePath(sceneName);
        if (!UpdateSceneResourcePathsOnDisk(sceneFilePath, replacements, &updatedCount))
        {
            CDebug::LogWarnning(L"Scene reference update failed after rename: " + sceneFilePath.wstring());
            continue;
        }

        if (updatedCount > 0u)
            CDebug::Log(L"Updated renamed asset references in scene: " + sceneName + L" (" + to_wstring(updatedCount) + L")");
    }
}

static string SanitizeItemName(const string& name)
{
    string sanitized = CEngineString::Trim(name);
    for (char& c : sanitized)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return sanitized;
}

static string StripPreservedExtension(const string& name, const string& extension)
{
    if (name.empty() || extension.empty())
        return name;

    const string lowerName = ToLowerCopy(name);
    const string lowerExt = ToLowerCopy(extension);
    if (lowerName.size() <= lowerExt.size())
        return name;

    if (lowerName.compare(lowerName.size() - lowerExt.size(), lowerExt.size(), lowerExt) != 0)
        return name;

    return name.substr(0, name.size() - extension.size());
}

static _bool IsPathInsideRoot(const fs::path& path, const fs::path& root)
{
    const fs::path relative = path.lexically_normal().lexically_relative(root.lexically_normal());
    if (relative.empty())
        return false;

    const string rel = NormalizeSlashPath(relative.generic_string());
    return rel == "." || (rel != ".." && rel.rfind("../", 0) != 0);
}

static vector<pair<fs::path, fs::path>> CollectFileRenamePairs(const fs::path& oldPath, const fs::path& newPath)
{
    vector<pair<fs::path, fs::path>> pairs;

    error_code ec;
    if (fs::is_regular_file(oldPath, ec))
    {
        pairs.push_back({ oldPath, newPath });
        return pairs;
    }

    ec.clear();
    if (!fs::is_directory(oldPath, ec))
        return pairs;

    for (const auto& entry : fs::recursive_directory_iterator(oldPath, ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file())
            continue;

        const fs::path relative = entry.path().lexically_relative(oldPath);
        pairs.push_back({ entry.path(), newPath / relative });
    }

    return pairs;
}

struct AssociatedBinaryRename
{
    fs::path oldPath;
    fs::path newPath;
};

static void AppendAssociatedBinaryRenamesForFile(const fs::path& oldFilePath, const fs::path& newFilePath, vector<AssociatedBinaryRename>& outRenames)
{
    const string ext = ToLowerCopy(oldFilePath.extension().string());
    const string oldPrefix = oldFilePath.parent_path().filename().string() + "_" + oldFilePath.stem().string();
    const string newPrefix = newFilePath.parent_path().filename().string() + "_" + newFilePath.stem().string();
    if (oldPrefix == newPrefix)
        return;

    struct BinaryMapping { string dir; string ext; };
    vector<BinaryMapping> mappings;

    if (ext == ".fbx")
    {
        mappings.push_back({ "BinaryAssets/MeshData", ".meshdata" });
        mappings.push_back({ "BinaryAssets/SkinnedMeshData", ".skinneddata" });
        mappings.push_back({ "BinaryAssets/AnimationClipData", ".animdata" });
    }
    else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".tif" || ext == ".tiff")
    {
        mappings.push_back({ "BinaryAssets/TextureData", ".dds" });
    }
    else if (ext == ".animatorcontroller")
    {
        mappings.push_back({ "BinaryAssets/AnimatorControllerData", ".acdata" });
    }
    else if (ext == ".ttf" || ext == ".otf")
    {
        mappings.push_back({ "BinaryAssets/FontData", ".spritefont" });
    }

    for (const auto& mapping : mappings)
    {
        const fs::path oldBinaryPath = fs::path(mapping.dir) / (oldPrefix + mapping.ext);
        error_code ec;
        if (!fs::exists(oldBinaryPath, ec) || ec)
            continue;

        const fs::path newBinaryPath = fs::path(mapping.dir) / (newPrefix + mapping.ext);
        outRenames.push_back({ oldBinaryPath, newBinaryPath });
    }
}

static _bool ValidateAssociatedBinaryRenames(const vector<AssociatedBinaryRename>& renames, string* outError)
{
    unordered_set<string> targetSet;

    for (const auto& rename : renames)
    {
        const string oldKey = ToLowerCopy(NormalizeSlashPath(rename.oldPath.generic_string()));
        const string newKey = ToLowerCopy(NormalizeSlashPath(rename.newPath.generic_string()));
        if (oldKey == newKey)
            continue;

        if (!targetSet.insert(newKey).second)
        {
            if (outError)
                *outError = "Rename would create duplicate binary file names.";
            return false;
        }

        error_code ec;
        if (fs::exists(rename.newPath, ec) && !ec)
        {
            if (outError)
                *outError = "Associated binary already exists: " + rename.newPath.filename().string();
            return false;
        }
    }

    return true;
}

static void ApplyAssociatedBinaryRenames(const vector<AssociatedBinaryRename>& renames)
{
    for (const auto& rename : renames)
    {
        error_code ec;
        fs::create_directories(rename.newPath.parent_path(), ec);

        ec.clear();
        fs::rename(rename.oldPath, rename.newPath, ec);
        if (ec)
            CDebug::LogWarnning(L"Associated binary rename failed: " + rename.oldPath.wstring() + L" -> " + rename.newPath.wstring());
    }
}

CProjectBox::CProjectBox()
	: m_bRequestDelete(false)
	, m_createTargetDir("")
	, m_bRequestCreateAC(false)
	, m_newACName({})
	, m_createFolderTargetDir("")
	, m_bRequestCreateFolder(false)
	, m_newFolderName({})
	, m_renameTargetPath("")
	, m_bRequestRename(false)
	, m_newRenameName({})
{
}

CProjectBox::~CProjectBox()
{
	OnDestroy();
}

CProjectBox* CProjectBox::Create()
{
	CProjectBox* newBox = new CProjectBox();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"Project";

	return newBox;
}

void CProjectBox::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	_float width = static_cast<_float>(editorOption.projectWidth);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImVec2 panelSize = ImVec2(width, viewport->Size.y - editorOption.topBarHeight);

	ImGui::SetNextWindowPos
	(
		ImVec2(viewport->Pos.x + viewport->Size.x - editorOption.hierachyWidth - editorOption.inspectorWidth, viewport->Pos.y + editorOption.topBarHeight),
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

	m_vFlatVisibleFiles = std::move(m_vFlatVisibleFilesBuilding);
	m_vFlatVisibleFilesBuilding.clear();

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.f);

    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::InputTextWithHint("##ProjectSearch", "Type to filter...", m_searchBuffer.data(), m_searchBuffer.size());
    ImGui::Separator();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        !m_vSelectedPaths.empty())
    {
        m_vPendingDeletePaths.clear();
        for (const string& p : m_vSelectedPaths)
        {
            error_code ec;
            if (fs::exists(p, ec) && !ec)
                m_vPendingDeletePaths.push_back(p);
        }
        if (!m_vPendingDeletePaths.empty())
            m_bRequestDelete = true;
        else
        {
            m_vSelectedPaths.clear();
            CEditor::GetInstance().Set_SelectedAssetPath(fs::path());
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_F2, false) &&
        m_vSelectedPaths.size() == 1 &&
        !ImGui::IsPopupOpen("RenameItemPopup"))
    {
        const fs::path renameTarget = fs::path(*m_vSelectedPaths.begin());
        BeginRename(renameTarget);
    }

	RenderAssetFoldersHierarchy();
	RenderBinaryFoldersHierarchy();

	ImGui::PopStyleVar();

	if (m_bRequestDelete)
	{
		ImGui::OpenPopup("ConfirmDeletePopup");
		m_bRequestDelete = false;
	}

	if (ImGui::BeginPopupModal("ConfirmDeletePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (m_vPendingDeletePaths.size() == 1)
		{
			error_code ec;
			const bool isDir = fs::is_directory(m_vPendingDeletePaths[0], ec);
			ImGui::Text(isDir
				? "Are you sure you want to delete this folder and its contents?"
				: "Are you sure you want to delete this file?");
		}
		else
		{
			ImGui::Text("Are you sure you want to delete %d items?", (int)m_vPendingDeletePaths.size());
		}
		ImGui::Separator();

		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			for (const string& p : m_vPendingDeletePaths)
				DeleteAssociatedBinaries(fs::path(p));

			for (const string& p : m_vPendingDeletePaths)
			{
				error_code ec;
				if (fs::is_directory(p, ec))
					fs::remove_all(p, ec);
				else
					fs::remove(p, ec);

				if (ec)
					CDebug::LogError(L"Delete failed: " + fs::path(p).wstring());
				else
					m_vSelectedPaths.erase(p);
			}
			m_vPendingDeletePaths.clear();
			if (m_vSelectedPaths.empty())
				CEditor::GetInstance().Set_SelectedAssetPath(fs::path());
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			m_vPendingDeletePaths.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	RenderRenamePopup();
    RenderCreateAnimatorControllerPopup();
	RenderCreateFolderPopup();

	ImGui::End();
}

void CProjectBox::OnDestroy()
{
}

void CProjectBox::RenderAssetFoldersHierarchy()
{
	static const fs::path rootPath = L"../Assets";

	RenderDirectoryRecursive(rootPath);
}

void CProjectBox::RenderBinaryFoldersHierarchy()
{
	static const fs::path rootPath = L"BinaryAssets";

	RenderDirectoryRecursive(rootPath);
}

void CProjectBox::RenderDirectoryRecursive(const fs::path& _dirPath)
{
    const string query = m_searchBuffer.data();
    const _bool hasQuery = !query.empty();
    const _bool hasMatches = DirectoryMatchesQuery(_dirPath, query);
    if (!hasMatches)
        return;

    string folderName = _dirPath.filename().string();
    string folderLabel = folderName + "##" + _dirPath.string();
	const _bool isRoot = (_dirPath == fs::path(L"../Assets") || _dirPath == fs::path(L"BinaryAssets"));
    const string folderPathStr = _dirPath.string();

    m_vFlatVisibleFilesBuilding.push_back(folderPathStr);

    if (hasQuery)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    ImGuiTreeNodeFlags folderNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (m_vSelectedPaths.count(folderPathStr) > 0)
        folderNodeFlags |= ImGuiTreeNodeFlags_Selected;

    _bool opened = ImGui::TreeNodeEx(folderLabel.c_str(), folderNodeFlags);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        const bool shiftHeld = ImGui::GetIO().KeyShift;
        const bool ctrlHeld = ImGui::GetIO().KeyCtrl;

        if (shiftHeld && !m_strLastClickedPath.empty())
        {
            auto itA = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), m_strLastClickedPath);
            auto itB = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), folderPathStr);
            if (itA != m_vFlatVisibleFiles.end() && itB != m_vFlatVisibleFiles.end())
            {
                if (itA > itB) std::swap(itA, itB);
                m_vSelectedPaths.clear();
                for (auto it = itA; it != itB + 1; ++it)
                    m_vSelectedPaths.insert(*it);
            }
            else
            {
                m_vSelectedPaths.clear();
                m_vSelectedPaths.insert(folderPathStr);
                m_strLastClickedPath = folderPathStr;
            }
        }
        else if (ctrlHeld)
        {
            if (m_vSelectedPaths.count(folderPathStr) > 0)
                m_vSelectedPaths.erase(folderPathStr);
            else
                m_vSelectedPaths.insert(folderPathStr);
            m_strLastClickedPath = folderPathStr;
        }
        else
        {
            m_vSelectedPaths.clear();
            m_vSelectedPaths.insert(folderPathStr);
            m_strLastClickedPath = folderPathStr;
        }
        CEditor::GetInstance().Set_SelectedAssetPath(_dirPath);
    }

    if (ImGui::BeginPopupContextItem(("FolderCtx##" + _dirPath.string()).c_str()))
    {
        const bool isMultiContext = m_vSelectedPaths.count(folderPathStr) > 0 && m_vSelectedPaths.size() > 1;

        if (ImGui::BeginMenu("Create"))
        {
			if (ImGui::MenuItem("Folder"))
			{
				m_createFolderTargetDir = _dirPath;
				m_bRequestCreateFolder = true;
			}
            if (ImGui::MenuItem("AnimatorController"))
            {
                m_createTargetDir = _dirPath;
                m_bRequestCreateAC = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Show in Explorer"))
            ShowInExplorer(_dirPath, false);

        if (isRoot || isMultiContext)
        {
            ImGui::BeginDisabled();
            ImGui::MenuItem("Rename", "F2");
            ImGui::EndDisabled();
        }
        else if (ImGui::MenuItem("Rename", "F2"))
        {
            BeginRename(_dirPath);
        }

        ImGui::Separator();

        {
            vector<string> sources = isMultiContext
                ? vector<string>(m_vSelectedPaths.begin(), m_vSelectedPaths.end())
                : vector<string>{ folderPathStr };

            auto fbxPred = [](const string& e) { return e == "fbx"; };
            auto texPred = [](const string& e) { return e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" || e == "tga" || e == "tif" || e == "tiff"; };
            auto fontPred = [](const string& e) { return e == "ttf" || e == "otf"; };
            auto acPred = [](const string& e) { return e == "animatorcontroller"; };

            auto fbxConvertRel = [](const string& t) -> wstring {
                wstring rel = fs::path(t).wstring();
                rel = CEngineString::Erase(rel, L"../Assets\\");
                rel = CEngineString::Replace(rel, L"\\", L"/");
                return rel;
            };

            if (ImGui::Selectable("Create Mesh Data"))
            {
                auto targets = CollectFilesFromPaths(sources, fbxPred);
                BatchConvertFiles(targets, "Mesh Data", [&](const string& t) {
                    return CResources::GetInstance().ConvertFBXToMeshBufferData(fbxConvertRel(t));
                });
            }

            if (ImGui::Selectable("Create Skinned Data"))
            {
                auto targets = CollectFilesFromPaths(sources, fbxPred);
                BatchConvertFiles(targets, "Skinned Data", [&](const string& t) {
                    return CResources::GetInstance().ConvertFBXToSkinnedBufferData(fbxConvertRel(t));
                });
            }

            if (ImGui::Selectable("Create Animation Data"))
            {
                auto targets = CollectFilesFromPaths(sources, fbxPred);
                BatchConvertFiles(targets, "Animation Data", [&](const string& t) {
                    return CResources::GetInstance().ConvertFBXToAnimationClipData(fbxConvertRel(t));
                });
            }

            if (ImGui::Selectable("Create Texture Data"))
            {
                auto targets = CollectFilesFromPaths(sources, texPred);
                BatchConvertFiles(targets, "Texture Data", [&](const string& t) {
                    return CResources::GetInstance().ConvertImageToDDS(fs::path(t).wstring());
                });
            }

            if (ImGui::Selectable("Create Font Data"))
            {
                auto targets = CollectFilesFromPaths(sources, fontPred);
                BatchConvertFiles(targets, "Font Data", [&](const string& t) {
                    return CResources::GetInstance().ConvertOTFTTFToSpriteFont(fs::path(t).wstring());
                });
            }

            if (ImGui::Selectable("Build Binary"))
            {
                auto targets = CollectFilesFromPaths(sources, acPred);
                BatchConvertFiles(targets, "Build Binary", [](const string& t) {
                    wstring rel = fs::path(t).wstring();
                    rel = CEngineString::Erase(rel, L"../Assets\\");
                    rel = CEngineString::Replace(rel, L"\\", L"/");
                    return CResources::GetInstance().ConvertAnimatorControllerToBinary(rel);
                });
            }
        }

        ImGui::Separator();

		if (isRoot)
		{
			ImGui::BeginDisabled();
			ImGui::MenuItem("Delete");
			ImGui::EndDisabled();
		}
		else if (ImGui::MenuItem("Delete"))
		{
			m_vPendingDeletePaths.clear();
            if (isMultiContext)
            {
                for (const string& p : m_vSelectedPaths)
                    m_vPendingDeletePaths.push_back(p);
            }
            else
            {
			    m_vPendingDeletePaths.push_back(folderPathStr);
            }
			m_bRequestDelete = true;
		}

        ImGui::EndPopup();
    }

    if (opened)
    {
        for (const auto& entry : fs::directory_iterator(_dirPath))
        {
            if (entry.is_directory())
            {
                RenderDirectoryRecursive(entry.path());
            }
            else if (entry.is_regular_file())
            {
                string filename = entry.path().filename().string();
                if (!ContainsCaseInsensitive(filename, query))
                    continue;
                string buttonId = filename + "##" + entry.path().string();
                const string pathStr = entry.path().string();

                m_vFlatVisibleFilesBuilding.push_back(pathStr);

                const string lowerExtension = ToLowerCopy(entry.path().extension().string());
                const _bool isFbx = (lowerExtension == ".fbx");
                const _bool isImage = (lowerExtension == ".png" || lowerExtension == ".jpg" || lowerExtension == ".jpeg" || lowerExtension == ".bmp" || lowerExtension == ".tga" || lowerExtension == ".tif" || lowerExtension == ".tiff");
                _bool isSelected = m_vSelectedPaths.count(pathStr) > 0;
                _bool fbxNodeOpened = false;
                _bool imageNodeOpened = false;

                if (isFbx || isImage)
                {
                    ImGuiTreeNodeFlags fbxFlags = ImGuiTreeNodeFlags_OpenOnArrow;
                    if (isSelected)
                        fbxFlags |= ImGuiTreeNodeFlags_Selected;

                    _bool treeNodeOpened = ImGui::TreeNodeEx(buttonId.c_str(), fbxFlags);

                    if (isFbx) fbxNodeOpened = treeNodeOpened;
                    if (isImage) imageNodeOpened = treeNodeOpened;

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        const bool shiftHeld = ImGui::GetIO().KeyShift;
                        const bool ctrlHeld = ImGui::GetIO().KeyCtrl;

                        if (shiftHeld && !m_strLastClickedPath.empty())
                        {
                            auto itA = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), m_strLastClickedPath);
                            auto itB = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), pathStr);
                            if (itA != m_vFlatVisibleFiles.end() && itB != m_vFlatVisibleFiles.end())
                            {
                                if (itA > itB) std::swap(itA, itB);
                                m_vSelectedPaths.clear();
                                for (auto it = itA; it != itB + 1; ++it)
                                    m_vSelectedPaths.insert(*it);
                            }
                            else
                            {
                                m_vSelectedPaths.clear();
                                m_vSelectedPaths.insert(pathStr);
                                m_strLastClickedPath = pathStr;
                            }
                        }
                        else if (ctrlHeld)
                        {
                            if (isSelected)
                                m_vSelectedPaths.erase(pathStr);
                            else
                                m_vSelectedPaths.insert(pathStr);
                            m_strLastClickedPath = pathStr;
                        }
                        else
                        {
                            m_vSelectedPaths.clear();
                            m_vSelectedPaths.insert(pathStr);
                            m_strLastClickedPath = pathStr;
                        }
                        CEditor::GetInstance().Set_SelectedAssetPath(entry.path());
                    }
                }
                else
                {
                if (ImGui::Selectable(buttonId.c_str(), isSelected))
                {
                    const bool shiftHeld = ImGui::GetIO().KeyShift;
                    const bool ctrlHeld = ImGui::GetIO().KeyCtrl;

                    if (shiftHeld && !m_strLastClickedPath.empty())
                    {
                        auto itA = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), m_strLastClickedPath);
                        auto itB = std::find(m_vFlatVisibleFiles.begin(), m_vFlatVisibleFiles.end(), pathStr);
                        if (itA != m_vFlatVisibleFiles.end() && itB != m_vFlatVisibleFiles.end())
                        {
                            if (itA > itB) std::swap(itA, itB);
                            m_vSelectedPaths.clear();
                            for (auto it = itA; it != itB + 1; ++it)
                                m_vSelectedPaths.insert(*it);
                        }
                        else
                        {
                            m_vSelectedPaths.clear();
                            m_vSelectedPaths.insert(pathStr);
                            m_strLastClickedPath = pathStr;
                        }
                    }
                    else if (ctrlHeld)
                    {
                        if (isSelected)
                            m_vSelectedPaths.erase(pathStr);
                        else
                            m_vSelectedPaths.insert(pathStr);
                        m_strLastClickedPath = pathStr;
                    }
                    else
                    {
                        m_vSelectedPaths.clear();
                        m_vSelectedPaths.insert(pathStr);
                        m_strLastClickedPath = pathStr;
                    }
                    CEditor::GetInstance().Set_SelectedAssetPath(entry.path());
                }
                }

                if (!isFbx && !isImage && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    const fs::path binaryRoot = fs::path(L"BinaryAssets");
                    const fs::path relativeToBinary = entry.path().lexically_relative(binaryRoot);
                    if (!relativeToBinary.empty() && relativeToBinary.native()[0] != L'.')
                        ShowInExplorer(entry.path(), true);
                    else
                        CEditor::GetInstance().OpenAsset(entry.path());
                }

                if (isFbx)
                {
                    const string assetRelPath = BuildAssetRelativePath(entry.path());
                    if (!assetRelPath.empty() && ImGui::BeginDragDropSource())
                    {
                        ImGui::SetDragDropPayload("ProjectAssetPath", assetRelPath.c_str(), assetRelPath.size() + 1u);
                        ImGui::TextUnformatted(filename.c_str());
                        ImGui::TextUnformatted("Drop into Editor Scene to spawn");
                        ImGui::EndDragDropSource();
                    }
                }

                if (ImGui::BeginPopupContextItem(buttonId.c_str()))
                {
                    auto splitName = CEngineString::Split(filename, ".");
                    const string path = entry.path().string();
                    const string fileName = splitName[0];
                    const string extension = splitName.size() > 1 ? splitName[1] : "";

                    const bool isMultiContext = m_vSelectedPaths.count(pathStr) > 0 && m_vSelectedPaths.size() > 1;

                    vector<string> selectedSources = isMultiContext
                        ? vector<string>(m_vSelectedPaths.begin(), m_vSelectedPaths.end())
                        : vector<string>{ pathStr };

                    if (ImGui::Selectable("Log info"))
                    {
                        uintmax_t sizeInBytes = fs::file_size(entry.path());
                        double sizeKB = sizeInBytes / 1024.0;

                        CDebug::Log("Name: " + fileName);
                        CDebug::Log("Type: " + extension);
                        CDebug::Log("Path: " + path);
                        CDebug::Log("Size: " + to_string(sizeKB) + "kb");
                    }

                    if (ImGui::Selectable("Show in Explorer"))
                        ShowInExplorer(entry.path(), true);

                    if (isMultiContext)
                    {
                        ImGui::BeginDisabled();
                        ImGui::MenuItem("Rename", "F2");
                        ImGui::EndDisabled();
                    }
                    else if (ImGui::MenuItem("Rename", "F2"))
                    {
                        BeginRename(entry.path());
                    }

                    if (extension == "animatorcontroller")
                    {
                        if (ImGui::Selectable("Open"))
                            CEditor::GetInstance().OpenAnimatorController(entry.path());

                        if (ImGui::Selectable("Build Binary"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) { return e == "animatorcontroller"; });
                            BatchConvertFiles(targets, "Build Binary", [](const string& t) {
                                wstring rel = fs::path(t).wstring();
                                rel = CEngineString::Erase(rel, L"../Assets\\");
                                rel = CEngineString::Replace(rel, L"\\", L"/");
                                return CResources::GetInstance().ConvertAnimatorControllerToBinary(rel);
                            });
                        }
                    }

                    if (extension == "fbx")
                    {
                        auto fbxConvertRel = [](const string& t) -> wstring {
                            wstring rel = fs::path(t).wstring();
                            rel = CEngineString::Erase(rel, L"../Assets\\");
                            rel = CEngineString::Replace(rel, L"\\", L"/");
                            return rel;
                        };

                        if (ImGui::Selectable("Create Mesh Data"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) { return e == "fbx"; });
                            BatchConvertFiles(targets, "Mesh Data", [&](const string& t) {
                                return CResources::GetInstance().ConvertFBXToMeshBufferData(fbxConvertRel(t));
                            });
                        }

                        if (ImGui::Selectable("Create Skinned Data"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) { return e == "fbx"; });
                            BatchConvertFiles(targets, "Skinned Data", [&](const string& t) {
                                return CResources::GetInstance().ConvertFBXToSkinnedBufferData(fbxConvertRel(t));
                            });
                        }

                        if (ImGui::Selectable("Create Animation Data"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) { return e == "fbx"; });
                            BatchConvertFiles(targets, "Animation Data", [&](const string& t) {
                                return CResources::GetInstance().ConvertFBXToAnimationClipData(fbxConvertRel(t));
                            });
                        }
                    }

                    if (extension == "ttf" || extension == "otf")
                    {
                        if (ImGui::Selectable("Create Font Data"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) { return e == "ttf" || e == "otf"; });
                            BatchConvertFiles(targets, "Font Data", [](const string& t) {
                                return CResources::GetInstance().ConvertOTFTTFToSpriteFont(fs::path(t).wstring());
                            });
                        }
                    }

                    if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" || extension == "tga" || extension == "tif" || extension == "tiff")
                    {
                        if (ImGui::Selectable("Create Texture Data"))
                        {
                            auto targets = CollectFilesFromPaths(selectedSources,
                                [](const string& e) {
                                    return e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" || e == "tga" || e == "tif" || e == "tiff";
                                });
                            BatchConvertFiles(targets, "Texture Data", [](const string& t) {
                                return CResources::GetInstance().ConvertImageToDDS(fs::path(t).wstring());
                            });
                        }
                    }

                    if (ImGui::Selectable("Delete"))
                    {
                        m_vPendingDeletePaths.clear();
                        if (isMultiContext)
                        {
                            for (const string& p : m_vSelectedPaths)
                                m_vPendingDeletePaths.push_back(p);
                        }
                        else
                        {
                            m_vPendingDeletePaths.push_back(pathStr);
                        }
                        m_bRequestDelete = true;
                    }

                    ImGui::EndPopup();
                }

                if (isFbx)
                {
                    if (fbxNodeOpened)
                    {
                        const string fbxStem = entry.path().stem().string();
                        const string fbxFolder = entry.path().parent_path().filename().string();
                        const string binaryPrefix = fbxFolder + "_" + fbxStem;

                        struct BinaryEntry
                        {
                            string label;
                            fs::path path;
                        };
                        vector<BinaryEntry> binaryEntries;

                        {
                            fs::path meshPath = fs::path("BinaryAssets/MeshData") / (binaryPrefix + ".meshdata");
                            error_code ec;
                            if (fs::exists(meshPath, ec))
                                binaryEntries.push_back({ binaryPrefix + ".meshdata", meshPath });
                        }
                        {
                            fs::path skinnedPath = fs::path("BinaryAssets/SkinnedMeshData") / (binaryPrefix + ".skinneddata");
                            error_code ec;
                            if (fs::exists(skinnedPath, ec))
                                binaryEntries.push_back({ binaryPrefix + ".skinneddata", skinnedPath });
                        }
                        {
                            fs::path animPath = fs::path("BinaryAssets/AnimationClipData") / (binaryPrefix + ".animdata");
                            error_code ec;
                            if (fs::exists(animPath, ec))
                                binaryEntries.push_back({ binaryPrefix + ".animdata", animPath });
                        }

                        if (binaryEntries.empty())
                        {
                            ImGui::TextDisabled("No binary data");
                        }
                        else
                        {
                            for (const auto& be : binaryEntries)
                            {
                                const string childId = be.label + "##" + be.path.string();
                                const string childPathStr = be.path.string();
                                _bool childSelected = m_vSelectedPaths.count(childPathStr) > 0;

                                if (ImGui::Selectable(childId.c_str(), childSelected))
                                {
                                    m_vSelectedPaths.clear();
                                    m_vSelectedPaths.insert(childPathStr);
                                    m_strLastClickedPath = childPathStr;
                                    CEditor::GetInstance().Set_SelectedAssetPath(be.path);
                                }

                                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                    ShowInExplorer(be.path, true);
                            }
                        }

                        ImGui::TreePop();
                    }
                }

                if (isImage)
                {
                    if (imageNodeOpened)
                    {
                        const string imgStem = entry.path().stem().string();
                        const string imgFolder = entry.path().parent_path().filename().string();
                        const string ddsBinaryName = imgFolder + "_" + imgStem + ".dds";

                        fs::path ddsPath = fs::path("BinaryAssets/TextureData") / ddsBinaryName;
                        error_code ec;
                        if (fs::exists(ddsPath, ec))
                        {
                            const string childId = ddsBinaryName + "##" + ddsPath.string();
                            const string childPathStr = ddsPath.string();
                            _bool childSelected = m_vSelectedPaths.count(childPathStr) > 0;

                            if (ImGui::Selectable(childId.c_str(), childSelected))
                            {
                                m_vSelectedPaths.clear();
                                m_vSelectedPaths.insert(childPathStr);
                                m_strLastClickedPath = childPathStr;
                                CEditor::GetInstance().Set_SelectedAssetPath(ddsPath);
                            }

                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                ShowInExplorer(ddsPath, true);
                        }
                        else
                        {
                            ImGui::TextDisabled("No binary data");
                        }

                        ImGui::TreePop();
                    }
                }
            }
        }

        ImGui::TreePop();
    }
}

void CProjectBox::RenderCreateAnimatorControllerPopup()
{
    if (m_bRequestCreateAC)
    {
        ImGui::OpenPopup("CreateAnimatorControllerPopup");
        m_bRequestCreateAC = false;
    }

    if (ImGui::BeginPopupModal("CreateAnimatorControllerPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create AnimatorController");
        ImGui::Separator();

        ImGui::Text("Folder:");
        ImGui::SameLine();
        ImGui::Text("%s", m_createTargetDir.string().c_str());

        ImGui::InputText("Name", m_newACName.data(), m_newACName.size());

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            CreateAnimatorControllerFile(m_createTargetDir, m_newACName.data());
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static string SanitizeFileName(const string& name)
{
    string sanitized = SanitizeItemName(name);
    if (sanitized.empty())
        sanitized = "NewAnimatorController";
    return sanitized;
}

void CProjectBox::RenderRenamePopup()
{
    if (m_bRequestRename)
    {
        ImGui::OpenPopup("RenameItemPopup");
        m_bRequestRename = false;
    }

    if (!ImGui::BeginPopupModal("RenameItemPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    error_code ec;
    const _bool isDirectory = fs::is_directory(m_renameTargetPath, ec);
    const string extension = isDirectory ? "" : m_renameTargetPath.extension().string();

    ImGui::Text(isDirectory ? "Rename Folder" : "Rename File");
    ImGui::Separator();

    ImGui::Text("Target:");
    ImGui::SameLine();
    ImGui::Text("%s", m_renameTargetPath.string().c_str());

    if (!extension.empty())
    {
        ImGui::Text("Extension:");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", extension.c_str());
    }

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    _bool applyRename = ImGui::InputText("Name", m_newRenameName.data(), m_newRenameName.size(), ImGuiInputTextFlags_EnterReturnsTrue);

    if (!m_strRenameError.empty())
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", m_strRenameError.c_str());

    ImGui::Separator();

    if (ImGui::Button("Rename", ImVec2(120, 0)))
        applyRename = true;

    if (applyRename)
    {
        string errorMessage;
        if (RenamePath(m_renameTargetPath, m_newRenameName.data(), &errorMessage))
        {
            m_strRenameError.clear();
            ImGui::CloseCurrentPopup();
        }
        else
        {
            m_strRenameError = errorMessage;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
        m_strRenameError.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void CProjectBox::RenderCreateFolderPopup()
{
	if (m_bRequestCreateFolder)
	{
		ImGui::OpenPopup("CreateFolderPopup");
		m_bRequestCreateFolder = false;
	}

	if (ImGui::BeginPopupModal("CreateFolderPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Create Folder");
		ImGui::Separator();

		ImGui::Text("Folder:");
		ImGui::SameLine();
		ImGui::Text("%s", m_createFolderTargetDir.string().c_str());

		ImGui::InputText("Name", m_newFolderName.data(), m_newFolderName.size());
		ImGui::Separator();

		if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			CreateFolder(m_createFolderTargetDir, m_newFolderName.data());
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void CProjectBox::BeginRename(const fs::path& targetPath)
{
    error_code ec;
    if (!fs::exists(targetPath, ec) || ec)
        return;

    const fs::path normalized = targetPath.lexically_normal();
    if (normalized == fs::path(L"../Assets").lexically_normal() ||
        normalized == fs::path(L"BinaryAssets").lexically_normal())
        return;

    m_renameTargetPath = targetPath;
    m_bRequestRename = true;
    m_strRenameError.clear();

    string initialName = fs::is_directory(targetPath, ec)
        ? targetPath.filename().string()
        : targetPath.stem().string();

    m_newRenameName.fill(0);
    strncpy_s(m_newRenameName.data(), m_newRenameName.size(), initialName.c_str(), _TRUNCATE);

    m_vSelectedPaths.clear();
    m_vSelectedPaths.insert(targetPath.string());
    m_strLastClickedPath = targetPath.string();
    CEditor::GetInstance().Set_SelectedAssetPath(targetPath);
}

_bool CProjectBox::RenamePath(const fs::path& sourcePath, const string& requestedName, string* outError)
{
    auto setError = [&](const string& message)
        {
            if (outError)
                *outError = message;
            return false;
        };

    error_code ec;
    if (!fs::exists(sourcePath, ec) || ec)
        return setError("Selected item no longer exists.");

    const _bool isDirectory = fs::is_directory(sourcePath, ec);
    const _bool isFile = fs::is_regular_file(sourcePath, ec);
    if (!isDirectory && !isFile)
        return setError("Unsupported item type.");

    string sanitizedName = SanitizeItemName(requestedName);
    if (isFile)
        sanitizedName = StripPreservedExtension(sanitizedName, sourcePath.extension().string());

    if (sanitizedName.empty())
        return setError("Name cannot be empty.");

    const fs::path targetPath = isFile
        ? (sourcePath.parent_path() / (sanitizedName + sourcePath.extension().string()))
        : (sourcePath.parent_path() / sanitizedName);
    const vector<pair<fs::path, fs::path>> resourcePathRenames = CollectFileRenamePairs(sourcePath, targetPath);

    const string sourceKey = ToLowerCopy(NormalizeSlashPath(sourcePath.lexically_normal().generic_string()));
    const string targetKey = ToLowerCopy(NormalizeSlashPath(targetPath.lexically_normal().generic_string()));
    if (sourceKey == targetKey)
    {
        m_vSelectedPaths.clear();
        m_vSelectedPaths.insert(targetPath.string());
        m_strLastClickedPath = targetPath.string();
        CEditor::GetInstance().Set_SelectedAssetPath(targetPath);
        return true;
    }

    ec.clear();
    if (fs::exists(targetPath, ec) && !ec)
        return setError("An item with the same name already exists.");

    vector<AssociatedBinaryRename> binaryRenames;
    vector<pair<fs::path, fs::path>> sceneReferenceRenames;
    if (IsPathInsideRoot(sourcePath, fs::path(L"../Assets")))
    {
        sceneReferenceRenames = resourcePathRenames;
        for (const auto& filePair : resourcePathRenames)
            AppendAssociatedBinaryRenamesForFile(filePair.first, filePair.second, binaryRenames);

        if (!ValidateAssociatedBinaryRenames(binaryRenames, outError))
            return false;
    }

    ec.clear();
    fs::rename(sourcePath, targetPath, ec);
    if (ec)
        return setError("Rename failed.");

    ApplyAssociatedBinaryRenames(binaryRenames);
    UpdateSceneReferencesForRenamePairs(sceneReferenceRenames);
    RemapLoadedSceneResourcePaths(resourcePathRenames);

    m_vSelectedPaths.clear();
    m_vSelectedPaths.insert(targetPath.string());
    m_strLastClickedPath = targetPath.string();
    CEditor::GetInstance().Set_SelectedAssetPath(targetPath);

    CDebug::Log(L"Renamed: " + sourcePath.wstring() + L" -> " + targetPath.wstring());
    return true;
}

void CProjectBox::CreateAnimatorControllerFile(const fs::path& dir, const string& name)
{
    string safeName = SanitizeFileName(name);

    fs::path outPath = dir / (safeName + ".animatorcontroller");

    if (fs::exists(outPath))
    {
        CDebug::LogError(L"AnimatorController create failed - file exists: " + outPath.wstring());
        return;
    }

    fs::create_directories(outPath.parent_path());

    string text = MakeAnimatorControllerTemplateText(safeName);

    ofstream ofs(outPath, ios::binary);
    if (!ofs.is_open())
    {
        CDebug::LogError(L"AnimatorController create failed - cannot open: " + outPath.wstring());
        return;
    }

    ofs.write(text.data(), (streamsize)text.size());
    ofs.close();

    m_vSelectedPaths.clear();
    m_vSelectedPaths.insert(outPath.string());
    m_strLastClickedPath = outPath.string();
    CEditor::GetInstance().Set_SelectedAssetPath(outPath);

    CDebug::Log("Created AnimatorController: " + outPath.string());
}

void CProjectBox::CreateFolder(const fs::path& dir, const string& name)
{
	string safeName = SanitizeFileName(name);

	fs::path outPath = dir / safeName;

	if (fs::exists(outPath))
	{
		CDebug::LogError(L"Folder create failed - already exists: " + outPath.wstring());
		return;
	}

	error_code ec;
	fs::create_directories(outPath, ec);
	if (ec)
	{
		CDebug::LogError(L"Folder create failed: " + outPath.wstring());
		return;
	}

	m_vSelectedPaths.clear();
	m_vSelectedPaths.insert(outPath.string());
	m_strLastClickedPath = outPath.string();
	CEditor::GetInstance().Set_SelectedAssetPath(outPath);

	CDebug::Log("Created Folder: " + outPath.string());
}

string CProjectBox::MakeAnimatorControllerTemplateText(const string& controllerName)
{
    string t;
    t += "# AnimatorController v1\n";
    t += "name=" + controllerName + "\n";
    t += "entry=Idle\n";
    t += "\n";

    t += "[parameters]\n";
    t += "float speed=0\n";
    t += "trigger attack\n";
    t += "bool isDead=false\n";
    t += "\n";

    t += "[state Idle]\n";
    t += "motion=Idle_Clip\n";
    t += "speedMul=1\n";
    t += "pos=100,100\n";
    t += "\n";

    t += "[state Run]\n";
    t += "motion=Run_Clip\n";
    t += "speedMul=1\n";
    t += "pos=320,100\n";
    t += "\n";

    t += "[transition Idle->Run]\n";
    t += "blend=0.15\n";
    t += "cond=speed>0.1\n";
    t += "\n";

    t += "[transition Run->Idle]\n";
    t += "blend=0.15\n";
    t += "cond=speed<=0.1\n";
    t += "\n";

    t += "[any]\n";
    t += "to=Attack\n";
    t += "blend=0.08\n";
    t += "cond=attack\n";
    t += "\n";

    t += "[state Attack]\n";
    t += "motion=Attack_Clip\n";
    t += "speedMul=1\n";
    t += "pos=220,260\n";
    t += "\n";

    t += "[transition Attack->Idle]\n";
    t += "blend=0.12\n";
    t += "exitTime=0.9\n";
    t += "\n";

    return t;
}
