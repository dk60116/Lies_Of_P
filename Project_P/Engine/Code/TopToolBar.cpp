#include "epch.h"
#include "TopToolBar.h"

CTopToolBar::CTopToolBar()
{
}

CTopToolBar::~CTopToolBar()
{
}

CTopToolBar* CTopToolBar::Create()
{
	CTopToolBar* newBox = new CTopToolBar();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"ToolBar";

	return newBox;
}

void CTopToolBar::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	_float width = static_cast<_float>(viewport->Size.x);

	_float padding = static_cast<_float>(editorOption.topBarHeight) - 32.f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));

	ImGui::SetNextWindowPos
	(
		ImVec2(0, padding),
		0,
		ImVec2(0.0f, 0.0f)
	);

	ImGui::SetNextWindowContentSize(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(width, static_cast<_float>(editorOption.topBarHeight)));

	ImGui::Begin
	(
		CEngineString::WStringToString(m_strBoxName).c_str(),
		nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings
	);

	ShowSelectSceneButton();
	Show2DButton();
	ShowFPS();

	ImGui::End();

	ImGui::PopStyleVar();
}

void CTopToolBar::OnDestroy()
{
}

void CTopToolBar::ShowSelectSceneButton()
{
	if (ImGui::Button("Scenes"))
	{
		ImGui::OpenPopup("ScenePopup");
	}

	auto sceneList = CSceneManager::GetInstance().Get_SceneList();

	if (ImGui::BeginPopup("ScenePopup"))
	{
		for (TRAVERSAL_ITER(sceneList, it))
		{
			if (ImGui::Button(CEngineString::WStringToString((*it).second->Get_SceneName()).c_str()))
			{
				CSceneManager::GetInstance().LoadScene((*it).second);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

void CTopToolBar::Show2DButton()
{
	ImGui::SameLine();

	if (ImGui::Button("2D"))
	{
		CCamera* editorCam = CSceneManager::GetInstance().Get_EditorCamera();
		editorCam->Get_Transform()->Set_PositionZ(-999999.f);
		editorCam->Get_Transform()->Set_EulerAngles(vector3::zero());
		editorCam->Set_ViewMode(CCamera::ViewMode::ORTHOGRAPHIC);
	}
}

void CTopToolBar::ShowFPS()
{
	static float timeAccumulator = 0.0f;
	_float fps = ImGui::GetIO().Framerate;
	static char fpsText[32] = "FPS: 0.0";

	timeAccumulator += DELTA_TIME;

	if (timeAccumulator >= 1.0f)
	{
		float fps = ImGui::GetIO().Framerate;
		snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
		timeAccumulator = 0.0f;
	}

	// 텍스트 크기
	ImVec2 textSize = ImGui::CalcTextSize(fpsText);

	_float rightMargin = 8.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textSize.x - rightMargin);

	// 텍스트 출력
	ImGui::SameLine();
	_float textWidth = ImGui::CalcTextSize(fpsText).x;
	_float availableWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - textWidth);
	ImGui::TextUnformatted(fpsText);
}
