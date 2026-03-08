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

CProjectBox::CProjectBox()
	: m_strCurrentSelectedFilePath("")
	, m_strPendingDeletePath("")
	, m_bRequestDelete(false)
	, m_bPendingDeleteIsDirectory(false)
	, m_createTargetDir("")
	, m_bRequestCreateAC(false)
	, m_newACName({})
	, m_createFolderTargetDir("")
	, m_bRequestCreateFolder(false)
	, m_newFolderName({})
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

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.f);

    ImGui::Text("Search");
    ImGui::SameLine();
    ImGui::InputTextWithHint("##ProjectSearch", "Type to filter...", m_searchBuffer.data(), m_searchBuffer.size());
    ImGui::Separator();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        !m_strCurrentSelectedFilePath.empty())
    {
        error_code deleteTargetEc;
        if (fs::exists(m_strCurrentSelectedFilePath, deleteTargetEc) && !deleteTargetEc)
        {
            m_strPendingDeletePath = m_strCurrentSelectedFilePath;
            m_bPendingDeleteIsDirectory = fs::is_directory(m_strCurrentSelectedFilePath, deleteTargetEc);
            m_bRequestDelete = true;
        }
        else
        {
            m_strCurrentSelectedFilePath.clear();
            CEditor::GetInstance().Set_SelectedAssetPath(fs::path());
        }
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
		ImGui::Text(m_bPendingDeleteIsDirectory
			? "Are you sure you want to delete this folder and its contents?"
			: "Are you sure you want to delete this file?");
		ImGui::Separator();

		if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            error_code ec;
            if (m_bPendingDeleteIsDirectory)
                fs::remove_all(m_strPendingDeletePath, ec);
            else
                fs::remove(m_strPendingDeletePath, ec);

            const fs::path deletedPath = m_strPendingDeletePath;
            if (ec)
                CDebug::LogError(L"Delete failed: " + deletedPath.wstring());
            else if (m_strCurrentSelectedFilePath == deletedPath)
            {
                m_strCurrentSelectedFilePath.clear();
                CEditor::GetInstance().Set_SelectedAssetPath(fs::path());
            }

            m_strPendingDeletePath.clear();
			m_bPendingDeleteIsDirectory = false;
            ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			m_strPendingDeletePath.clear();
			m_bPendingDeleteIsDirectory = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

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

    if (hasQuery)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    const ImGuiTreeNodeFlags folderNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
    _bool opened = ImGui::TreeNodeEx(folderLabel.c_str(), folderNodeFlags);

    if (ImGui::BeginPopupContextItem(("FolderCtx##" + _dirPath.string()).c_str()))
    {
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

		if (isRoot)
		{
			ImGui::BeginDisabled();
			ImGui::MenuItem("Delete");
			ImGui::EndDisabled();
		}
		else if (ImGui::MenuItem("Delete"))
		{
			m_strPendingDeletePath = _dirPath.string();
			m_bPendingDeleteIsDirectory = true;
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

                _bool isSelected = (m_strCurrentSelectedFilePath == entry.path().string());
                if (ImGui::Selectable(buttonId.c_str(), isSelected))
                {
                    m_strCurrentSelectedFilePath = entry.path().string();
                    CEditor::GetInstance().Set_SelectedAssetPath(entry.path());
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    const fs::path binaryRoot = fs::path(L"BinaryAssets");
                    const fs::path relativeToBinary = entry.path().lexically_relative(binaryRoot);
                    if (!relativeToBinary.empty() && relativeToBinary.native()[0] != L'.')
                        ShowInExplorer(entry.path(), true);
                    else
                        CEditor::GetInstance().OpenAsset(entry.path());
                }

                if (ImGui::BeginPopupContextItem(buttonId.c_str()))
                {
                    auto splitName = CEngineString::Split(filename, ".");
                    const string path = entry.path().string();
                    const string fileName = splitName[0];
                    const string extension = splitName.size() > 1 ? splitName[1] : "";

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

                    if (extension == "animatorcontroller")
                    {
                        if (ImGui::Selectable("Open"))
                        {
                            CEditor::GetInstance().OpenAnimatorController(entry.path());
                        }

                        if (ImGui::Selectable("Build Binary"))
                        {
                            wstring rel = entry.path().wstring();
                            rel = CEngineString::Erase(rel, L"../Assets\\");
                            rel = CEngineString::Replace(rel, L"\\", L"/");

                            CResources::GetInstance().ConvertAnimatorControllerToBinary(rel);
                        }
                    }

                    if (extension == "fbx")
                    {
                        wstring rel = entry.path().wstring();
                        rel = CEngineString::Erase(rel, L"../Assets\\");
                        rel = CEngineString::Replace(rel, L"\\", L"/");

                        if (ImGui::Selectable("Create Mesh Data"))
                            CResources::GetInstance().ConvertFBXToMeshBufferData(rel);

                        if (ImGui::Selectable("Create Skinned Data"))
                            CResources::GetInstance().ConvertFBXToSkinnedBufferData(rel);

                        if (ImGui::Selectable("Create Animation Data"))
                            CResources::GetInstance().ConvertFBXToAnimationClipData(rel);
                    }

                    if (extension == "ttf" || extension == "otf")
                    {
                        wstring pathW = entry.path().wstring();
                        if (ImGui::Selectable("Create Font Data"))
                            CResources::GetInstance().ConvertOTFTTFToSpriteFont(pathW);
                    }

                    if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" || extension == "tga" || extension == "tif" || extension == "tiff")
                    {
                        wstring pathW = entry.path().wstring();
                        if (ImGui::Selectable("Create Texture Data"))
                            CResources::GetInstance().ConvertImageToDDS(pathW);
                    }

                    if (ImGui::Selectable("Delete"))
                    {
                        m_strPendingDeletePath = entry.path().string();
                        m_bRequestDelete = true;
						m_bPendingDeleteIsDirectory = false;
                    }

                    ImGui::EndPopup();
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
    string n = name;

    if (n.empty()) n = "NewAnimatorController";
    for (char& c : n)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return n;
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

    m_strCurrentSelectedFilePath = outPath.string();
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

	m_strCurrentSelectedFilePath = outPath.string();
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
