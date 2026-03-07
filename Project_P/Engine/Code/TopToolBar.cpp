#include "epch.h"
#include "TopToolBar.h"

CTopToolBar::CTopToolBar()
	: m_bProjectSettingsWindowOpen(false)
	, m_iProjectSettingsSelection(0)
	, m_fPendingFixedTimeStep(0.02f)
	, m_fPendingTimeScale(1.f)
	, m_iPendingShadowQuality(0)
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
	ShowEditMenu();
	ShowViewMenu();
	Show2DButton();
	ShowPlayButtons();
	ShowFPS();

	ImGui::End();

	ShowProjectSettingsWindow();

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


void CTopToolBar::ShowEditMenu()
{
	ImGui::SameLine();

	if (ImGui::Button("Edit"))
		ImGui::OpenPopup("EditMenuPopup");

	if (ImGui::BeginPopup("EditMenuPopup"))
	{
		if (ImGui::MenuItem("ProjectSetting"))
		{
			CSceneManager& sceneManager = CSceneManager::GetInstance();
			auto timeSetting = sceneManager.Get_TimeSetting();
			auto lightSetting = sceneManager.Get_LightSetting();
			m_fPendingFixedTimeStep = timeSetting.fixedTimeStep;
			m_fPendingTimeScale = timeSetting.timeSclae;
			m_iPendingShadowQuality = static_cast<_int>(lightSetting.shadowQuality);
			m_bProjectSettingsWindowOpen = true;
		}

		ImGui::EndPopup();
	}
}

void CTopToolBar::ShowViewMenu()
{
	ImGui::SameLine();

	if (ImGui::Button("View"))
		ImGui::OpenPopup("ViewMenuPopup");

	if (ImGui::BeginPopup("ViewMenuPopup"))
	{
		_bool showCollider = CEditor::GetInstance().IsColliderGizmoVisible();
		if (ImGui::MenuItem("Collider", nullptr, showCollider))
			CEditor::GetInstance().SetColliderGizmoVisible(!showCollider);

		ImGui::EndPopup();
	}
}

void CTopToolBar::ShowProjectSettingsWindow()
{
	if (!m_bProjectSettingsWindowOpen)
		return;

	ImGui::SetNextWindowSize(ImVec2(720.f, 420.f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Project Settings", &m_bProjectSettingsWindowOpen))
	{
		const _float buttonAreaHeight = ImGui::GetFrameHeightWithSpacing() + 6.f;
		ImGui::BeginChild("ProjectSettingsContent", ImVec2(0.f, -buttonAreaHeight), false);
		ImGui::BeginChild("ProjectSettingsCategoryList", ImVec2(180.f, 0.f), true);
		if (ImGui::Selectable("Time", m_iProjectSettingsSelection == 0))
			m_iProjectSettingsSelection = 0;
		if (ImGui::Selectable("Light", m_iProjectSettingsSelection == 1))
			m_iProjectSettingsSelection = 1;
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("ProjectSettingsEditor", ImVec2(0.f, 0.f), true);
		if (m_iProjectSettingsSelection == 0)
			ShowProjectSettingsTime();
		else if (m_iProjectSettingsSelection == 1)
			ShowProjectSettingsLight();
		ImGui::EndChild();
		ImGui::EndChild();

		if (ImGui::Button("Save"))
		{
			CSceneManager& sceneManager = CSceneManager::GetInstance();
			sceneManager.Set_FixedTimeStep(m_fPendingFixedTimeStep);
			sceneManager.Set_TimeScale(m_fPendingTimeScale);
			sceneManager.Set_ShadowQuality(static_cast<CSceneManager::shadowQualityOptions>(m_iPendingShadowQuality));
			sceneManager.SaveEngineSettings();
		}
	}

	ImGui::End();
}

void CTopToolBar::ShowProjectSettingsTime()
{
	ImGui::Text("Time");
	ImGui::Separator();

	if (ImGui::DragFloat("Fixed Time Step", &m_fPendingFixedTimeStep, 0.0001f, 0.0001f, 1.f, "%.4f"))
	{
		if (m_fPendingFixedTimeStep < 0.0001f)
			m_fPendingFixedTimeStep = 0.0001f;
	}

	if (ImGui::DragFloat("Time Scale", &m_fPendingTimeScale, 0.01f, 0.f, 10.f, "%.2f"))
	{
		if (m_fPendingTimeScale < 0.f)
			m_fPendingTimeScale = 0.f;
	}
}


void CTopToolBar::ShowProjectSettingsLight()
{
	ImGui::Text("Light");
	ImGui::Separator();

	const char* qualityNames[] = { "Low", "Middle", "High", "SuperHigh", "Ultra" };
	if (m_iPendingShadowQuality < 0)
		m_iPendingShadowQuality = 0;
	if (m_iPendingShadowQuality > 4)
		m_iPendingShadowQuality = 4;

	ImGui::Combo("Shadow Quality", &m_iPendingShadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames));

	_uint shadowMapSize = 1024u;
	switch (m_iPendingShadowQuality)
	{
	case static_cast<_int>(CSceneManager::shadowQualityOptions::Low):
		shadowMapSize = 1024u;
		break;
	case static_cast<_int>(CSceneManager::shadowQualityOptions::Middle):
		shadowMapSize = 2048u;
		break;
	case static_cast<_int>(CSceneManager::shadowQualityOptions::High):
		shadowMapSize = 4096u;
		break;
	case static_cast<_int>(CSceneManager::shadowQualityOptions::SuperHigh):
		shadowMapSize = 8192u;
		break;
	case static_cast<_int>(CSceneManager::shadowQualityOptions::Ultra):
		shadowMapSize = 16384u;
		break;
	}

	ImGui::Text("Shadow Map Size: %u", shadowMapSize);
}

void CTopToolBar::ShowPlayButtons()
{
	CSceneManager& sceneManager = CSceneManager::GetInstance();

	ImGui::SameLine();

	const char* playLabel = "\xE2\x96\xB6";
	const char* stopLabel = "\xE2\x96\xA0";
	const char* pauseLabel = "||";
	const char* stepLabel = "\xE2\x96\xB6\xEF\xBD\x9C";

	const _float buttonWidth = 36.f;
	const ImVec2 buttonSize(buttonWidth, 0.f);
	const _float spacing = ImGui::GetStyle().ItemSpacing.x;
	const _float totalWidth = buttonWidth * 3.f + spacing * 2.f;
	_float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
	if (startX < 0.f)
		startX = 0.f;
	ImGui::SetCursorPosX(startX);

	if (!sceneManager.IsPlayMode())
	{
		if (ImGui::Button(playLabel, buttonSize))
			sceneManager.PlayScene();
	}
	else
	{
		if (ImGui::Button(stopLabel, buttonSize))
			sceneManager.StopScene();
	}

	ImGui::SameLine();

	if (sceneManager.IsPlaying())
	{
		if (ImGui::Button(pauseLabel, buttonSize))
			sceneManager.PauseScene();
	}
	else if (sceneManager.IsPaused())
	{
		if (ImGui::Button(playLabel, buttonSize))
			sceneManager.PlayScene();
	}
	else
	{
		ImGui::BeginDisabled();
		ImGui::Button(pauseLabel, buttonSize);
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (sceneManager.IsPaused())
	{
		if (ImGui::Button(stepLabel, buttonSize))
			sceneManager.RequestStepFrame();
	}
	else
	{
		ImGui::BeginDisabled();
		ImGui::Button(stepLabel, buttonSize);
		ImGui::EndDisabled();
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
		editorCam->SetViewMode(CCamera::ViewMode::Orthographic);
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

	// ÅØ½ºÆ® Å©±â
	ImVec2 textSize = ImGui::CalcTextSize(fpsText);

	_float rightMargin = 8.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textSize.x - rightMargin);

	// ÅØ½ºÆ® Ãâ·Â
	ImGui::SameLine();
	_float textWidth = ImGui::CalcTextSize(fpsText).x;
	_float availableWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - textWidth);
	ImGui::TextUnformatted(fpsText);
}
