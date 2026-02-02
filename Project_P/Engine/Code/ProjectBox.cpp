#include "epch.h"
#include "ProjectBox.h"

static void ShowInExplorer(const fs::path& path, const _bool selectItem)
{
#ifdef _WIN32
	if (selectItem && fs::is_regular_file(path))
	{
		wstring args = L"/select,\"" + path.wstring() + L"\"";
		HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
		if ((INT_PTR)result <= 32)
			CDebug::LogError(L"ShowInExplorer failed: " + path.wstring());
		return;
	}

	HINSTANCE result = ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if ((INT_PTR)result <= 32)
		CDebug::LogError(L"ShowInExplorer failed: " + path.wstring());
#else
	CDebug::LogError(L"ShowInExplorer is not implemented on this platform.");
#endif
}

CProjectBox::CProjectBox()
	: m_strCurrentSelectedFilePath("")
	, m_strPendingDeletePath("")
	, m_bRequestDelete(false)
	, m_createTargetDir("")
	, m_bRequestCreateAC(false)
	, m_newACName({})
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
		ImGuiWindowFlags_NoCollapse
	);

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.f);

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
		ImGui::Text("Are you sure you want to delete this file?");
		ImGui::Separator();

		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
            error_code ec;
            fs::remove(m_strPendingDeletePath, ec);
            if (ec)
                CDebug::LogError(L"Delete failed: " + m_strPendingDeletePath.wstring());

            m_strPendingDeletePath.clear();
            ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			m_strPendingDeletePath.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

    RenderCreateAnimatorControllerPopup();

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
    string folderName = _dirPath.filename().string();
    string folderLabel = folderName + "##" + _dirPath.string();

    _bool opened = ImGui::TreeNode(folderLabel.c_str());

    if (ImGui::BeginPopupContextItem(("FolderCtx##" + _dirPath.string()).c_str()))
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("AnimatorController"))
            {
                m_createTargetDir = _dirPath;
                m_bRequestCreateAC = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Show in Explorer"))
            ShowInExplorer(_dirPath, false);

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
                string buttonId = filename + "##" + entry.path().string();

                if (ImGui::Button(buttonId.c_str()))
                {
                    m_strCurrentSelectedFilePath = entry.path().string();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
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
                            // Assets 상대경로로 변환해서 Convert 호출 (다른 Convert와 동일 패턴)
                            wstring rel = entry.path().wstring();
                            rel = CEngineString::Erase(rel, L"../Assets\\");
                            rel = CEngineString::Replace(rel, L"\\", L"/");

                            CResources::GetInstance().ConvertAnimatorControllerToBinary(rel);
                        }
                    }

                    // 기존 fbx/ttf 변환 메뉴...
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

                    if (ImGui::Selectable("Delete"))
                    {
                        m_strPendingDeletePath = entry.path().string();
                        m_bRequestDelete = true;
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

void CProjectBox::CreateAnimatorControllerFile(const fs::path& dir, const string& name)
{
    string safeName = SanitizeFileName(name);

    fs::path outPath = dir / (safeName + ".animatorcontroller");

    if (fs::exists(outPath))
    {
        CDebug::LogError(L"AnimatorController create failed - file exists: " + outPath.wstring());
        return;
    }

    // 폴더 보장
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

    CDebug::Log("Created AnimatorController: " + outPath.string());
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
