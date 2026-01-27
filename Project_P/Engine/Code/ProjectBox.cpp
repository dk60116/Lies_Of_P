#include "epch.h"
#include "ProjectBox.h"

CProjectBox::CProjectBox()
	: m_strCurrentSelectedFilePath("")
	, m_strPendingDeletePath("")
	, m_bRequestDelete(false)
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
		m_bRequestDelete = false; // 한 번만 요청
	}

	if (ImGui::BeginPopupModal("ConfirmDeletePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Are you sure you want to delete this file?");
		ImGui::Separator();

		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			fs::remove(m_strPendingDeletePath);
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

	if (ImGui::TreeNode(folderName.c_str()))
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
					m_strCurrentSelectedFilePath = entry.path();
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

					if (extension == "fbx")
					{
						wstring path = CEngineString::Erase(entry.path().wstring(), L"../Assets\\");
						path = CEngineString::Replace(path, L"\\", L"/");

						if (ImGui::Selectable("Create Mesh Data"))
							CResources::GetInstance().ConvertFBXToMeshBufferData(path);

						if (ImGui::Selectable("Create Skinned Data"))
							CResources::GetInstance().ConvertFBXToSkinnedBufferData(path);

						if (ImGui::Selectable("Create Animation Data"))
							CResources::GetInstance().ConvertFBXToAnimationClipData(path);
					}

					if (extension == "ttf" || extension == "otf")
					{
						wstring path = entry.path().wstring();

						if (ImGui::Selectable("Create Font Data"))
							CResources::GetInstance().ConvertOTFTTFToSpriteFont(path);
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
