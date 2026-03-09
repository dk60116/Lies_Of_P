#include "epch.h"
#include "TopToolBar.h"
#include "MeshRenderer.h"

CTopToolBar::CTopToolBar()
	: m_bProjectSettingsWindowOpen(false)
	, m_iProjectSettingsSelection(0)
	, m_fPendingFixedTimeStep(0.02f)
	, m_fPendingTimeScale(1.f)
	, m_iPendingShadowQuality(0)
	, m_bSceneSettingsWindowOpen(false)
	, m_bGameStatusWindowOpen(false)
	, m_bNavigationWindowOpen(false)
	, m_bNavigationStaticOnly(false)
	, m_fNavAgentRadius(0.5f)
	, m_fNavAgentHeight(2.0f)
	, m_fNavMaxSlope(45.0f)
	, m_fNavStepHeight(0.4f)
	, m_fNavCellSize(0.166f)
	, m_fNavCellHeight(0.1f)
	, m_strNavigationBakeStatus(L"Ready")
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
	ShowSceneMenu();
	ShowEditMenu();
	ShowAIMenu();
	ShowViewMenu();
	Show2DButton();
	ShowPlayButtons();
	ShowFPS();

	ImGui::End();

	ShowProjectSettingsWindow();
	ShowSceneSettingsWindow();
	ShowGameStatusWindow();
	ShowNavigationWindow();

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

void CTopToolBar::ShowSceneMenu()
{
	ImGui::SameLine();

	if (ImGui::Button("Scene"))
		ImGui::OpenPopup("SceneMenuPopup");

	if (ImGui::BeginPopup("SceneMenuPopup"))
	{
		if (ImGui::MenuItem("Scene Setting"))
			m_bSceneSettingsWindowOpen = true;

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

		_bool showMeshCollider = CEditor::GetInstance().IsMeshColliderGizmoVisible();
		if (ImGui::MenuItem("Mesh Collider", nullptr, showMeshCollider))
			CEditor::GetInstance().SetMeshColliderGizmoVisible(!showMeshCollider);

		_bool showNavigation = CEditor::GetInstance().IsNavigationGizmoVisible();
		if (ImGui::MenuItem("Navigation", nullptr, showNavigation))
			CEditor::GetInstance().SetNavigationGizmoVisible(!showNavigation);

		if (ImGui::MenuItem("Game Status", nullptr, m_bGameStatusWindowOpen))
			m_bGameStatusWindowOpen = !m_bGameStatusWindowOpen;

		ImGui::EndPopup();
	}
}

void CTopToolBar::ShowAIMenu()
{
	ImGui::SameLine();
	if (ImGui::Button("AI"))
		ImGui::OpenPopup("AIMenuPopup");
	if (ImGui::BeginPopup("AIMenuPopup"))
	{
		if (ImGui::MenuItem("Navigation"))
			m_bNavigationWindowOpen = true;
		ImGui::EndPopup();
	}
}
void CTopToolBar::ShowNavigationWindow()
{
	if (!m_bNavigationWindowOpen)
		return;
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	ImGui::SetNextWindowSize(ImVec2(560.f, 420.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Navigation", &m_bNavigationWindowOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}
	if (!scene)
	{
		ImGui::TextUnformatted("No active scene.");
		ImGui::End();
		return;
	}
	_int sourceObjectCount = 0;
	_int sourceMeshCount = 0;
	_int sourceTriangleCount = 0;
	CollectNavigationBakeStats(sourceObjectCount, sourceMeshCount, sourceTriangleCount);
	if (ImGui::BeginTabBar("NavigationTabs"))
	{
		if (ImGui::BeginTabItem("Bake"))
		{
			ImGui::TextUnformatted("Bake Settings");
			ImGui::Separator();
			ImGui::Checkbox("Navigation Static Only", &m_bNavigationStaticOnly);
			ImGui::DragFloat("Agent Radius", &m_fNavAgentRadius, 0.01f, 0.01f, 100.f, "%.2f");
			ImGui::DragFloat("Agent Height", &m_fNavAgentHeight, 0.01f, 0.01f, 100.f, "%.2f");
			ImGui::DragFloat("Max Slope", &m_fNavMaxSlope, 0.1f, 0.f, 89.f, "%.1f");
			ImGui::DragFloat("Step Height", &m_fNavStepHeight, 0.01f, 0.f, 100.f, "%.2f");
			ImGui::DragFloat("Cell Size", &m_fNavCellSize, 0.001f, 0.001f, 10.f, "%.3f");
			ImGui::DragFloat("Cell Height", &m_fNavCellHeight, 0.001f, 0.001f, 10.f, "%.3f");
			ImGui::Spacing();
			ImGui::TextUnformatted("Source Summary");
			ImGui::Separator();
			ImGui::Text("Objects: %d", sourceObjectCount);
			ImGui::Text("Meshes: %d", sourceMeshCount);
			ImGui::Text("Triangles: %d", sourceTriangleCount);
			ImGui::TextWrapped("NavigationStatic is configurable from the Inspector static dropdown.");
			ImGui::Spacing();
			if (ImGui::Button("Bake", ImVec2(120.f, 0.f)))
				PerformNavigationBake();
			ImGui::SameLine();
			ImGui::Text("Status: %s", CEngineString::WStringToString(m_strNavigationBakeStatus).c_str());
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Object"))
		{
			ImGui::TextWrapped("Object tab will be connected after the bake pipeline is in place.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Areas"))
		{
			ImGui::TextWrapped("Areas tab will be connected after agent/area data is defined.");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}
void CTopToolBar::ShowSceneSettingsWindow()
{
	if (!m_bSceneSettingsWindowOpen)
		return;

	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();
	if (!currentScene)
	{
		m_bSceneSettingsWindowOpen = false;
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(420.f, 220.f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Scene Settings", &m_bSceneSettingsWindowOpen))
	{
		const CScene::EnviromentSettings& setting = currentScene->Get_EnviromentSetting();
		_float ambient = setting.ambient;
		_float shadowDist = setting.directionalLightShadowDist;
		_float shadowBias = setting.shadowBias;

		ImGui::Text("Scene");
		ImGui::Separator();
		ImGui::Text("%s", CEngineString::WStringToString(currentScene->Get_SceneName()).c_str());
		ImGui::Spacing();

		if (ImGui::DragFloat("Ambient", &ambient, 0.01f, 0.f, 1.f, "%.2f"))
			currentScene->Set_Ambient(ambient);

		if (ImGui::DragFloat("Directional Light Shadow Dist", &shadowDist, 1.f, 0.f, 10000.f, "%.1f"))
			currentScene->Set_DirectionalLightShadowDist(shadowDist);

		if (ImGui::DragFloat("Shadow Bias", &shadowBias, 0.0001f, 0.f, 1.f, "%.4f"))
			currentScene->Set_ShadwoBias(shadowBias);

		ImGui::Spacing();
		if (ImGui::Button("Save"))
		{
			wstring scenePath = L"../Assets/Scenes/" + currentScene->Get_SceneName() + L".scene";
			currentScene->SaveScene(scenePath);
		}
	}

	ImGui::End();
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

void CTopToolBar::ShowGameStatusWindow()
{
	if (!m_bGameStatusWindowOpen)
		return;

	ImGui::SetNextWindowSize(ImVec2(320.f, 180.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Game Status", &m_bGameStatusWindowOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}

	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	CCamera* camera = scene ? scene->Get_Camera() : nullptr;
	const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_GameViewport();
	const _uint screenWidth = vp ? static_cast<_uint>(vp->Width) : 0u;
	const _uint screenHeight = vp ? static_cast<_uint>(vp->Height) : 0u;

	if (!camera)
	{
		ImGui::TextUnformatted("No active game camera.");
		ImGui::End();
		return;
	}

	const CCamera::RenderStats& stats = camera->GetRenderStats();
	ImGui::Text("FPS: %d", CTime::GetInstance().Get_FPS());
	ImGui::Text("Batches: %u", stats.batches);
	ImGui::Text("Tris: %u", stats.tris);
	ImGui::Text("Verts: %u", stats.verts);
	ImGui::Text("Screen: %u x %u", screenWidth, screenHeight);
	ImGui::Text("VisibleSkinnedMeshes: %u", stats.visibleSkinnedMeshes);

	ImGui::End();
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

	// Text size
	ImVec2 textSize = ImGui::CalcTextSize(fpsText);

	_float rightMargin = 8.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textSize.x - rightMargin);

	// Text output
	ImGui::SameLine();
	_float textWidth = ImGui::CalcTextSize(fpsText).x;
	_float availableWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - textWidth);
	ImGui::TextUnformatted(fpsText);
}











void CTopToolBar::PerformNavigationBake()
{
	struct NavBakeTriangle
	{
		vector3 vertices[3] = {};
		vector3 normal = {};
		_float minY = 0.f;
		_float maxY = 0.f;
	};

	struct NavBakePolygon
	{
		vector<vector3> vertices = {};
		vector3 normal = {};
		_float minY = 0.f;
		_float maxY = 0.f;
	};

	struct NavBoundaryEdge
	{
		vector3 a = {};
		vector3 b = {};
		_float minY = 0.f;
		_float maxY = 0.f;
	};

	struct QuantizedVertexKey
	{
		long long x = 0;
		long long y = 0;
		long long z = 0;

		_bool operator==(const QuantizedVertexKey& rhs) const
		{
			return x == rhs.x && y == rhs.y && z == rhs.z;
		}

		_bool operator<(const QuantizedVertexKey& rhs) const
		{
			if (x != rhs.x)
				return x < rhs.x;
			if (y != rhs.y)
				return y < rhs.y;
			return z < rhs.z;
		}
	};

	struct QuantizedEdgeKey
	{
		QuantizedVertexKey a = {};
		QuantizedVertexKey b = {};

		_bool operator==(const QuantizedEdgeKey& rhs) const
		{
			return a == rhs.a && b == rhs.b;
		}
	};

	struct QuantizedVertexKeyHash
	{
		size_t operator()(const QuantizedVertexKey& key) const
		{
			const size_t hx0 = hash<long long>{}(key.x);
			const size_t hx1 = hash<long long>{}(key.y);
			const size_t hx2 = hash<long long>{}(key.z);
			return (((hx0 * 1315423911u) ^ hx1) * 1315423911u) ^ hx2;
		}
	};

	struct QuantizedEdgeKeyHash
	{
		size_t operator()(const QuantizedEdgeKey& key) const
		{
			const size_t hx0 = hash<long long>{}(key.a.x);
			const size_t hx1 = hash<long long>{}(key.a.y);
			const size_t hx2 = hash<long long>{}(key.a.z);
			const size_t hx3 = hash<long long>{}(key.b.x);
			const size_t hx4 = hash<long long>{}(key.b.y);
			const size_t hx5 = hash<long long>{}(key.b.z);
			return (((((hx0 * 1315423911u) ^ hx1) * 1315423911u) ^ hx2) * 1315423911u ^ hx3) * 1315423911u ^ hx4 ^ (hx5 << 1);
		}
	};

	auto ComputeTriangleCentroid = [](const NavBakeTriangle& triangle)
	{
		return vector3(
			(triangle.vertices[0].x + triangle.vertices[1].x + triangle.vertices[2].x) / 3.f,
			(triangle.vertices[0].y + triangle.vertices[1].y + triangle.vertices[2].y) / 3.f,
			(triangle.vertices[0].z + triangle.vertices[1].z + triangle.vertices[2].z) / 3.f);
	};

	struct TriangleSampleSet
	{
		vector3 points[4] = {};
		_int count = 0;
	};

	auto BuildTriangleSampleSet = [](const NavBakeTriangle& triangle)
	{
		TriangleSampleSet sampleSet = {};
		sampleSet.points[sampleSet.count++] = vector3(
			(triangle.vertices[0].x + triangle.vertices[1].x + triangle.vertices[2].x) / 3.f,
			(triangle.vertices[0].y + triangle.vertices[1].y + triangle.vertices[2].y) / 3.f,
			(triangle.vertices[0].z + triangle.vertices[1].z + triangle.vertices[2].z) / 3.f);
		sampleSet.points[sampleSet.count++] = vector3(
			triangle.vertices[0].x * 0.6f + triangle.vertices[1].x * 0.2f + triangle.vertices[2].x * 0.2f,
			triangle.vertices[0].y * 0.6f + triangle.vertices[1].y * 0.2f + triangle.vertices[2].y * 0.2f,
			triangle.vertices[0].z * 0.6f + triangle.vertices[1].z * 0.2f + triangle.vertices[2].z * 0.2f);
		sampleSet.points[sampleSet.count++] = vector3(
			triangle.vertices[0].x * 0.2f + triangle.vertices[1].x * 0.6f + triangle.vertices[2].x * 0.2f,
			triangle.vertices[0].y * 0.2f + triangle.vertices[1].y * 0.6f + triangle.vertices[2].y * 0.2f,
			triangle.vertices[0].z * 0.2f + triangle.vertices[1].z * 0.6f + triangle.vertices[2].z * 0.2f);
		sampleSet.points[sampleSet.count++] = vector3(
			triangle.vertices[0].x * 0.2f + triangle.vertices[1].x * 0.2f + triangle.vertices[2].x * 0.6f,
			triangle.vertices[0].y * 0.2f + triangle.vertices[1].y * 0.2f + triangle.vertices[2].y * 0.6f,
			triangle.vertices[0].z * 0.2f + triangle.vertices[1].z * 0.2f + triangle.vertices[2].z * 0.6f);
		return sampleSet;
	};

	auto ComputePointToSegmentDistanceSqXZ = [](const vector3& point, const vector3& a, const vector3& b)
	{
		const vector3 ab = vector3(b.x - a.x, 0.f, b.z - a.z);
		const vector3 ap = vector3(point.x - a.x, 0.f, point.z - a.z);
		const _float abLenSq = ab.lengthSq();
		if (abLenSq <= 1e-6f)
		{
			const _float dx = point.x - a.x;
			const _float dz = point.z - a.z;
			return dx * dx + dz * dz;
		}

		const _float t = clamp(vector3::dot(ap, ab) / abLenSq, 0.f, 1.f);
		const vector3 closest = vector3(a.x + ab.x * t, point.y, a.z + ab.z * t);
		const _float dx = point.x - closest.x;
		const _float dz = point.z - closest.z;
		return dx * dx + dz * dz;
	};

	auto TrySampleTriangleHeightAtXZ = [&](const NavBakeTriangle& triangle, const vector3& point, _float& outY)
	{
		const _float ax = triangle.vertices[0].x;
		const _float az = triangle.vertices[0].z;
		const _float bx = triangle.vertices[1].x;
		const _float bz = triangle.vertices[1].z;
		const _float cx = triangle.vertices[2].x;
		const _float cz = triangle.vertices[2].z;
		const _float px = point.x;
		const _float pz = point.z;
		const _float denom = ((bz - cz) * (ax - cx)) + ((cx - bx) * (az - cz));
		if (fabsf(denom) <= 1e-6f)
			return false;

		const _float w0 = (((bz - cz) * (px - cx)) + ((cx - bx) * (pz - cz))) / denom;
		const _float w1 = (((cz - az) * (px - cx)) + ((ax - cx) * (pz - cz))) / denom;
		const _float w2 = 1.f - w0 - w1;
		const _float baryTolerance = max(0.001f, min(m_fNavCellSize, m_fNavCellHeight) * 0.25f);
		if (w0 < -baryTolerance || w1 < -baryTolerance || w2 < -baryTolerance)
			return false;

		outY = triangle.vertices[0].y * w0 + triangle.vertices[1].y * w1 + triangle.vertices[2].y * w2;
		return true;
	};

	auto IsPointBlockedByObstacleRadius = [&](const vector3& point, const NavBakeTriangle& obstacle, const _float radiusSq)
	{
		if (point.y + m_fNavStepHeight < obstacle.minY)
			return false;
		if (point.y > obstacle.minY + m_fNavAgentHeight)
			return false;

		const _float baseEdgeTolerance = max(0.05f, m_fNavStepHeight + m_fNavCellHeight);
		for (_int edge = 0; edge < 3; ++edge)
		{
			const vector3& edgeA = obstacle.vertices[edge];
			const vector3& edgeB = obstacle.vertices[(edge + 1) % 3];
			if (fabsf(edgeA.y - obstacle.minY) > baseEdgeTolerance)
				continue;
			if (fabsf(edgeB.y - obstacle.minY) > baseEdgeTolerance)
				continue;
			if (ComputePointToSegmentDistanceSqXZ(point, edgeA, edgeB) <= radiusSq)
				return true;
		}

		return false;
	};

	const _float mergeVertexTolerance = max(0.001f, min(m_fNavCellSize, m_fNavCellHeight) * 0.1f);
	const _float mergeVertexToleranceSq = mergeVertexTolerance * mergeVertexTolerance;
	const _float mergePlaneTolerance = max(0.01f, m_fNavCellHeight * 0.5f);
	const _float mergeNormalDot = cosf(XMConvertToRadians(2.0f));
	const _float floorTolerance = max(0.01f, m_fNavCellHeight);
	const _float stepSampleOffset = max(max(m_fNavCellSize * 0.5f, mergeVertexTolerance * 4.f), 0.05f);

	auto ArePointsEqual = [&](const vector3& a, const vector3& b)
	{
		return (a - b).lengthSq() <= mergeVertexToleranceSq;
	};

	auto MakeVertexKey = [&](const vector3& point)
	{
		QuantizedVertexKey key = {};
		key.x = llround(point.x / mergeVertexTolerance);
		key.y = llround(point.y / mergeVertexTolerance);
		key.z = llround(point.z / mergeVertexTolerance);
		return key;
	};

	auto MakeEdgeKey = [&](const vector3& a, const vector3& b)
	{
		QuantizedEdgeKey key = {};
		key.a = MakeVertexKey(a);
		key.b = MakeVertexKey(b);
		if (key.b < key.a)
			swap(key.a, key.b);
		return key;
	};

	auto RebuildTriangle = [](NavBakeTriangle& triangle)
	{
		const vector3 edge0 = triangle.vertices[1] - triangle.vertices[0];
		const vector3 edge1 = triangle.vertices[2] - triangle.vertices[0];
		const vector3 faceNormal = vector3::Cross(edge0, edge1);
		if (faceNormal.lengthSq() <= 1e-6f)
			return false;

		triangle.normal = faceNormal.normalized();
		triangle.minY = min(triangle.vertices[0].y, min(triangle.vertices[1].y, triangle.vertices[2].y));
		triangle.maxY = max(triangle.vertices[0].y, max(triangle.vertices[1].y, triangle.vertices[2].y));
		return true;
	};

	struct WeldedVertexAccum
	{
		vector3 sum = {};
		_int count = 0;
	};

	auto BuildWeldedVertexPositions = [&](const vector<NavBakeTriangle>& triangles)
	{
		unordered_map<QuantizedVertexKey, WeldedVertexAccum, QuantizedVertexKeyHash> accumulations = {};
		accumulations.reserve(triangles.size() * 3);
		for (const NavBakeTriangle& triangle : triangles)
		{
			for (const vector3& vertex : triangle.vertices)
			{
				WeldedVertexAccum& accumulation = accumulations[MakeVertexKey(vertex)];
				accumulation.sum += vertex;
				++accumulation.count;
			}
		}

		unordered_map<QuantizedVertexKey, vector3, QuantizedVertexKeyHash> weldedPositions = {};
		weldedPositions.reserve(accumulations.size());
		for (const auto& accumulationPair : accumulations)
		{
			const WeldedVertexAccum& accumulation = accumulationPair.second;
			if (accumulation.count <= 0)
				continue;

			const _float invCount = 1.f / static_cast<_float>(accumulation.count);
			weldedPositions.emplace(accumulationPair.first, accumulation.sum * invCount);
		}

		return weldedPositions;
	};

	auto WeldTriangles = [&](vector<NavBakeTriangle>& triangles, const unordered_map<QuantizedVertexKey, vector3, QuantizedVertexKeyHash>& weldedPositions)
	{
		if (triangles.empty() || weldedPositions.empty())
			return;

		vector<NavBakeTriangle> weldedTriangles = {};
		weldedTriangles.reserve(triangles.size());
		for (NavBakeTriangle triangle : triangles)
		{
			for (_int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
			{
				const auto weldedPosition = weldedPositions.find(MakeVertexKey(triangle.vertices[vertexIndex]));
				if (weldedPosition != weldedPositions.end())
					triangle.vertices[vertexIndex] = weldedPosition->second;
			}

			if (!RebuildTriangle(triangle))
				continue;

			weldedTriangles.push_back(triangle);
		}

		triangles = move(weldedTriangles);
	};

	auto UpdatePolygonExtents = [](NavBakePolygon& polygon)
	{
		if (polygon.vertices.empty())
		{
			polygon.minY = 0.f;
			polygon.maxY = 0.f;
			return;
		}

		polygon.minY = polygon.vertices[0].y;
		polygon.maxY = polygon.vertices[0].y;
		for (const vector3& vertex : polygon.vertices)
		{
			polygon.minY = min(polygon.minY, vertex.y);
			polygon.maxY = max(polygon.maxY, vertex.y);
		}
	};

	auto ComputePolygonNormal = [](const vector<vector3>& vertices)
	{
		if (vertices.size() < 3)
			return vector3::up();

		const vector3 origin = vertices[0];
		for (size_t i = 1; i + 1 < vertices.size(); ++i)
		{
			const vector3 cross = vector3::Cross(vertices[i] - origin, vertices[i + 1] - origin);
			if (cross.lengthSq() <= 1e-6f)
				continue;

			vector3 normal = cross.normalized();
			if (normal.y < 0.f)
				normal *= -1.f;
			return normal;
		}

		return vector3::up();
	};

	auto RemoveRedundantVertices = [&](vector<vector3>& vertices, const vector3& planeNormal)
	{
		if (vertices.size() < 3)
			return;

		const _float collinearTolerance = 0.001f;
		_bool removed = true;
		while (removed && vertices.size() >= 3)
		{
			removed = false;
			for (size_t i = 0; i < vertices.size(); ++i)
			{
				const size_t prev = (i + vertices.size() - 1) % vertices.size();
				const size_t next = (i + 1) % vertices.size();
				const vector3& a = vertices[prev];
				const vector3& b = vertices[i];
				const vector3& c = vertices[next];

				if (ArePointsEqual(a, b) || ArePointsEqual(b, c))
				{
					vertices.erase(vertices.begin() + i);
					removed = true;
					break;
				}

				const vector3 edge0 = b - a;
				const vector3 edge1 = c - b;
				const _float edge0LenSq = edge0.lengthSq();
				const _float edge1LenSq = edge1.lengthSq();
				if (edge0LenSq <= 1e-6f || edge1LenSq <= 1e-6f)
				{
					vertices.erase(vertices.begin() + i);
					removed = true;
					break;
				}

				const vector3 turnCross = vector3::Cross(edge0, edge1);
				const _float normalizedTurn = fabsf(vector3::dot(turnCross, planeNormal)) / max(1e-6f, sqrtf(edge0LenSq * edge1LenSq));
				if (normalizedTurn <= collinearTolerance)
				{
					vertices.erase(vertices.begin() + i);
					removed = true;
					break;
				}
			}
		}
	};

	auto IsConvexPolygon = [](const vector<vector3>& vertices, const vector3& planeNormal)
	{
		if (vertices.size() < 3)
			return false;

		const _float convexTolerance = 0.001f;
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const vector3& a = vertices[i];
			const vector3& b = vertices[(i + 1) % vertices.size()];
			const vector3& c = vertices[(i + 2) % vertices.size()];
			const vector3 turnCross = vector3::Cross(b - a, c - b);
			if (turnCross.lengthSq() <= 1e-6f)
				continue;
			if (vector3::dot(turnCross, planeNormal) < -convexTolerance)
				return false;
		}

		return true;
	};

	auto ArePolygonsCoplanar = [&](const NavBakePolygon& lhs, const NavBakePolygon& rhs)
	{
		if (lhs.vertices.size() < 3 || rhs.vertices.size() < 3)
			return false;
		if (vector3::dot(lhs.normal, rhs.normal) < mergeNormalDot)
			return false;

		const vector3 lhsPlanePoint = lhs.vertices.front();
		for (const vector3& vertex : rhs.vertices)
		{
			if (fabsf(vector3::dot(vertex - lhsPlanePoint, lhs.normal)) > mergePlaneTolerance)
				return false;
		}

		const vector3 rhsPlanePoint = rhs.vertices.front();
		for (const vector3& vertex : lhs.vertices)
		{
			if (fabsf(vector3::dot(vertex - rhsPlanePoint, rhs.normal)) > mergePlaneTolerance)
				return false;
		}

		return true;
	};

	auto TryMergePolygons = [&](const NavBakePolygon& lhs, const size_t lhsEdge, const NavBakePolygon& rhs, const size_t rhsEdge, NavBakePolygon& outPolygon)
	{
		if (!ArePolygonsCoplanar(lhs, rhs))
			return false;

		const size_t lhsCount = lhs.vertices.size();
		const size_t rhsCount = rhs.vertices.size();
		if (lhsCount < 3 || rhsCount < 3)
			return false;

		const vector3& lhsEdgeStart = lhs.vertices[lhsEdge];
		const vector3& lhsEdgeEnd = lhs.vertices[(lhsEdge + 1) % lhsCount];
		const vector3& rhsEdgeStart = rhs.vertices[rhsEdge];
		const vector3& rhsEdgeEnd = rhs.vertices[(rhsEdge + 1) % rhsCount];
		if (!ArePointsEqual(lhsEdgeStart, rhsEdgeEnd) || !ArePointsEqual(lhsEdgeEnd, rhsEdgeStart))
			return false;

		vector<vector3> mergedVertices = {};
		mergedVertices.reserve(lhsCount + rhsCount - 2);
		mergedVertices.push_back(lhsEdgeStart);
		mergedVertices.push_back(lhsEdgeEnd);

		for (size_t index = (lhsEdge + 2) % lhsCount; index != lhsEdge; index = (index + 1) % lhsCount)
			mergedVertices.push_back(lhs.vertices[index]);
		for (size_t index = (rhsEdge + 2) % rhsCount; index != rhsEdge; index = (index + 1) % rhsCount)
			mergedVertices.push_back(rhs.vertices[index]);

		vector3 mergedNormal = ComputePolygonNormal(mergedVertices);
		RemoveRedundantVertices(mergedVertices, mergedNormal);
		if (mergedVertices.size() < 3)
			return false;

		mergedNormal = ComputePolygonNormal(mergedVertices);
		if (mergedNormal.lengthSq() <= 1e-6f)
			return false;
		if (vector3::dot(mergedNormal, lhs.normal) < 0.f)
		{
			reverse(mergedVertices.begin(), mergedVertices.end());
			mergedNormal = ComputePolygonNormal(mergedVertices);
		}
		if (!IsConvexPolygon(mergedVertices, mergedNormal))
			return false;

		outPolygon.vertices = move(mergedVertices);
		outPolygon.normal = mergedNormal;
		UpdatePolygonExtents(outPolygon);
		return true;
	};

	auto HasBlockingHeadroom = [&](const NavBakeTriangle& walkable, const vector<NavBakeTriangle>& sceneTriangles)
	{
		const TriangleSampleSet sampleSet = BuildTriangleSampleSet(walkable);
		_int blockedSampleCount = 0;
		for (_int sampleIndex = 0; sampleIndex < sampleSet.count; ++sampleIndex)
		{
			const vector3& samplePoint = sampleSet.points[sampleIndex];
			_float closestCeilingY = FLT_MAX;
			for (const NavBakeTriangle& sceneTriangle : sceneTriangles)
			{
				_float sampleY = 0.f;
				if (!TrySampleTriangleHeightAtXZ(sceneTriangle, samplePoint, sampleY))
					continue;
				if (sampleY <= samplePoint.y + floorTolerance)
					continue;
				closestCeilingY = min(closestCeilingY, sampleY);
			}

			if (closestCeilingY < samplePoint.y + m_fNavAgentHeight)
				++blockedSampleCount;
		}

		return blockedSampleCount == sampleSet.count;
	};

	auto TryFindNearbyWalkableHeight = [&](const vector<NavBakeTriangle>& walkables, const vector3& samplePoint, const NavBakeTriangle& ignoreTriangle, _float& outY)
	{
		_bool found = false;
		outY = -FLT_MAX;
		for (const NavBakeTriangle& candidate : walkables)
		{
			if (&candidate == &ignoreTriangle)
				continue;

			_float sampleY = 0.f;
			if (!TrySampleTriangleHeightAtXZ(candidate, samplePoint, sampleY))
				continue;

			if (!found || sampleY > outY)
			{
				outY = sampleY;
				found = true;
			}
		}

		return found;
	};

	auto IsEdgeStepReachable = [&](const vector<NavBakeTriangle>& walkables, const NavBakeTriangle& walkable, const _int edgeIndex)
	{
		const vector3& edgeA = walkable.vertices[edgeIndex];
		const vector3& edgeB = walkable.vertices[(edgeIndex + 1) % 3];
		vector3 edgeDir = edgeB - edgeA;
		edgeDir.y = 0.f;
		if (edgeDir.lengthSq() <= 1e-6f)
			return true;
		edgeDir = edgeDir.normalized();

		vector3 outward = vector3::Cross(walkable.normal, edgeDir);
		outward.y = 0.f;
		if (outward.lengthSq() <= 1e-6f)
			return true;
		outward = outward.normalized();

		const _float nearEdgeSampleOffset = max(mergeVertexTolerance * 2.f, 0.005f);
		const _float edgeSampleDistances[] =
		{
			nearEdgeSampleOffset,
			max(stepSampleOffset * 0.25f, nearEdgeSampleOffset),
			stepSampleOffset * 0.5f,
			stepSampleOffset,
			stepSampleOffset * 1.5f
		};
		constexpr _float edgeSampleTs[] = { 0.1f, 0.25f, 0.5f, 0.75f, 0.9f };
		for (_float edgeSampleT : edgeSampleTs)
		{
			const vector3 edgePoint = vector3::Lerp(edgeA, edgeB, edgeSampleT);
			for (_float sampleDistance : edgeSampleDistances)
			{
				const vector3 samplePoint = vector3(
					edgePoint.x + outward.x * sampleDistance,
					edgePoint.y,
					edgePoint.z + outward.z * sampleDistance);
				_float supportY = 0.f;
				if (!TryFindNearbyWalkableHeight(walkables, samplePoint, walkable, supportY))
					continue;
				if (fabsf(supportY - edgePoint.y) <= m_fNavStepHeight + floorTolerance)
					return true;
			}
		}

		return false;
	};

	auto IsPointBlockedByBoundaryRadius = [&](const vector3& point, const NavBoundaryEdge& boundary, const _float radiusSq)
	{
		const _float boundaryY = (boundary.minY + boundary.maxY) * 0.5f;
		if (fabsf(point.y - boundaryY) > m_fNavStepHeight + floorTolerance)
			return false;
		return ComputePointToSegmentDistanceSqXZ(point, boundary.a, boundary.b) <= radiusSq;
	};

	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!scene)
	{
		m_strNavigationBakeStatus = L"No active scene.";
		CEditor::GetInstance().ClearNavigationPreviewTriangles();
		return;
	}

	_int sourceObjectCount = 0;
	_int sourceMeshCount = 0;
	_int sourceTriangleCount = 0;
	_int rejectedSlopeTriangleCount = 0;
	_int passedStepHeightSlopeTriangleCount = 0;
	_int rejectedHeadroomTriangleCount = 0;
	_int rejectedObstacleTriangleCount = 0;
	_int rejectedStepTriangleCount = 0;
	_int mergedEdgeCount = 0;
	const _float maxSlopeCos = cosf(XMConvertToRadians(m_fNavMaxSlope));
	const _float agentRadiusSq = m_fNavAgentRadius * m_fNavAgentRadius;
	vector<NavBakeTriangle> sceneTriangles = {};
	vector<NavBakeTriangle> walkableTriangles = {};
	vector<NavBakeTriangle> obstacleTriangles = {};

	for (CGameObject* obj : scene->Get_ObjectList())
	{
		if (!obj || !obj->Is_SaveTarget())
			continue;
		if (m_bNavigationStaticOnly && !obj->IsStatic(CGameObject::STATIC_METHOD::NavigationStatic))
			continue;

		CMeshRenderer* meshRenderer = obj->GetComponent<CMeshRenderer>();
		if (!meshRenderer)
			continue;
		CMeshBuffer* meshBuffer = meshRenderer->Get_MeshBuffer();
		if (!meshBuffer)
			continue;

		vector<VertexTexNormalTangentBuffer> vertices = meshBuffer->Get_VertexBuffer();
		if (vertices.size() < 3)
			continue;
		vector<_uint> indices = meshBuffer->Get_IndexBuffer();
		const _matrix world = obj->Get_Transform()->Get_WorldMatrix();

		auto appendTriangle = [&](const _uint i0, const _uint i1, const _uint i2)
		{
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
				return;

			const VertexTexNormalTangentBuffer& v0 = vertices[i0];
			const VertexTexNormalTangentBuffer& v1 = vertices[i1];
			const VertexTexNormalTangentBuffer& v2 = vertices[i2];
			const _vector p0 = XMVector3Transform(XMLoadFloat3(&v0.position), world);
			const _vector p1 = XMVector3Transform(XMLoadFloat3(&v1.position), world);
			const _vector p2 = XMVector3Transform(XMLoadFloat3(&v2.position), world);
			const _vector edge0 = XMVectorSubtract(p1, p0);
			const _vector edge1 = XMVectorSubtract(p2, p0);
			const _vector faceNormal = XMVector3Cross(edge0, edge1);
			if (XMVectorGetX(XMVector3LengthSq(faceNormal)) <= 1e-6f)
				return;

			NavBakeTriangle triangle = {};
			triangle.vertices[0] = vector3(p0);
			triangle.vertices[1] = vector3(p1);
			triangle.vertices[2] = vector3(p2);
			triangle.normal = vector3(XMVector3Normalize(faceNormal));
			triangle.minY = min(triangle.vertices[0].y, min(triangle.vertices[1].y, triangle.vertices[2].y));
			triangle.maxY = max(triangle.vertices[0].y, max(triangle.vertices[1].y, triangle.vertices[2].y));
			sceneTriangles.push_back(triangle);

			const _float upDot = triangle.normal.y;
			if (upDot < maxSlopeCos)
			{
				const _float triangleHeight = triangle.maxY - triangle.minY;
				if (triangleHeight <= m_fNavStepHeight + floorTolerance)
				{
					++passedStepHeightSlopeTriangleCount;
					return;
				}

				obstacleTriangles.push_back(triangle);
				++rejectedSlopeTriangleCount;
				return;
			}

			walkableTriangles.push_back(triangle);
			++sourceTriangleCount;
		};

		if (!indices.empty())
		{
			for (size_t i = 0; i + 2 < indices.size(); i += 3)
				appendTriangle(indices[i], indices[i + 1], indices[i + 2]);
		}
		else
		{
			for (_uint i = 0; i + 2 < static_cast<_uint>(vertices.size()); i += 3)
				appendTriangle(i, i + 1, i + 2);
		}

		++sourceObjectCount;
		++sourceMeshCount;
	}

	if (!sceneTriangles.empty())
	{
		const auto weldedPositions = BuildWeldedVertexPositions(sceneTriangles);
		WeldTriangles(sceneTriangles, weldedPositions);
		WeldTriangles(walkableTriangles, weldedPositions);
		WeldTriangles(obstacleTriangles, weldedPositions);
		sourceTriangleCount = static_cast<_int>(walkableTriangles.size());
		rejectedSlopeTriangleCount = static_cast<_int>(obstacleTriangles.size());
	}

	vector<NavBakeTriangle> headroomFilteredWalkables = {};
	headroomFilteredWalkables.reserve(walkableTriangles.size());
	for (const NavBakeTriangle& walkable : walkableTriangles)
	{
		if (HasBlockingHeadroom(walkable, sceneTriangles))
		{
			++rejectedHeadroomTriangleCount;
			continue;
		}

		headroomFilteredWalkables.push_back(walkable);
	}

	if (headroomFilteredWalkables.empty())
	{
		m_strNavigationBakeStatus = L"No walkable triangles passed bake filters.";
		scene->SetNavigationTriangles({});
		CEditor::GetInstance().ClearNavigationPreviewTriangles();
		CDebug::LogWarnning(
			L"[Navigation] Bake skipped: no walkable triangles. Rejected by slope: " +
			to_wstring(rejectedSlopeTriangleCount) +
			L", Passed by step height: " + to_wstring(passedStepHeightSlopeTriangleCount) +
			L", Rejected by headroom: " + to_wstring(rejectedHeadroomTriangleCount));
		return;
	}

	struct EdgeOwner
	{
		size_t triangleIndex = 0;
		size_t edgeIndex = 0;
	};

	unordered_map<QuantizedEdgeKey, vector<EdgeOwner>, QuantizedEdgeKeyHash> stepEdgeOwners = {};
	stepEdgeOwners.reserve(headroomFilteredWalkables.size() * 4);
	for (size_t triangleIndex = 0; triangleIndex < headroomFilteredWalkables.size(); ++triangleIndex)
	{
		const NavBakeTriangle& walkable = headroomFilteredWalkables[triangleIndex];
		for (_int edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
		{
			const vector3& edgeA = walkable.vertices[edgeIndex];
			const vector3& edgeB = walkable.vertices[(edgeIndex + 1) % 3];
			stepEdgeOwners[MakeEdgeKey(edgeA, edgeB)].push_back({ triangleIndex, static_cast<size_t>(edgeIndex) });
		}
	}

	unordered_map<QuantizedEdgeKey, NavBoundaryEdge, QuantizedEdgeKeyHash> uniqueBoundaryEdges = {};
	for (const auto& edgeOwnerPair : stepEdgeOwners)
	{
		if (edgeOwnerPair.second.size() != 1)
			continue;

		const EdgeOwner& owner = edgeOwnerPair.second.front();
		const NavBakeTriangle& walkable = headroomFilteredWalkables[owner.triangleIndex];
		const _int edgeIndex = static_cast<_int>(owner.edgeIndex);
		if (IsEdgeStepReachable(headroomFilteredWalkables, walkable, edgeIndex))
			continue;

		NavBoundaryEdge boundary = {};
		boundary.a = walkable.vertices[edgeIndex];
		boundary.b = walkable.vertices[(edgeIndex + 1) % 3];
		boundary.minY = min(boundary.a.y, boundary.b.y);
		boundary.maxY = max(boundary.a.y, boundary.b.y);
		uniqueBoundaryEdges.insert_or_assign(edgeOwnerPair.first, boundary);
	}

	vector<NavBoundaryEdge> ledgeBoundaries = {};
	ledgeBoundaries.reserve(uniqueBoundaryEdges.size());
	for (const auto& boundaryPair : uniqueBoundaryEdges)
		ledgeBoundaries.push_back(boundaryPair.second);

	vector<NavBakePolygon> bakedPolygons = {};
	bakedPolygons.reserve(headroomFilteredWalkables.size());
	for (const NavBakeTriangle& walkable : headroomFilteredWalkables)
	{
		_bool blockedByErosion = false;
		if (agentRadiusSq > 1e-6f)
		{
			const TriangleSampleSet sampleSet = BuildTriangleSampleSet(walkable);
			_bool sawObstacleBlocker = false;
			_bool sawBoundaryBlocker = false;
			blockedByErosion = sampleSet.count > 0;
			for (_int sampleIndex = 0; sampleIndex < sampleSet.count; ++sampleIndex)
			{
				const vector3& samplePoint = sampleSet.points[sampleIndex];
				_bool sampleBlocked = false;
				for (const NavBakeTriangle& obstacle : obstacleTriangles)
				{
					if (!IsPointBlockedByObstacleRadius(samplePoint, obstacle, agentRadiusSq))
						continue;

					sawObstacleBlocker = true;
					sampleBlocked = true;
					break;
				}

				if (!sampleBlocked)
				{
					for (const NavBoundaryEdge& boundary : ledgeBoundaries)
					{
						if (!IsPointBlockedByBoundaryRadius(samplePoint, boundary, agentRadiusSq))
							continue;

						sawBoundaryBlocker = true;
						sampleBlocked = true;
						break;
					}
				}

				if (!sampleBlocked)
				{
					blockedByErosion = false;
					break;
				}
			}

			if (blockedByErosion)
			{
				if (sawObstacleBlocker)
					++rejectedObstacleTriangleCount;
				else if (sawBoundaryBlocker)
					++rejectedStepTriangleCount;
			}
		}

		if (blockedByErosion)
			continue;

		NavBakePolygon polygon = {};
		polygon.vertices.reserve(3);
		polygon.vertices.push_back(walkable.vertices[0]);
		polygon.vertices.push_back(walkable.vertices[1]);
		polygon.vertices.push_back(walkable.vertices[2]);
		polygon.normal = walkable.normal;
		polygon.minY = walkable.minY;
		polygon.maxY = walkable.maxY;
		bakedPolygons.push_back(move(polygon));
	}

	if (bakedPolygons.empty())
	{
		m_strNavigationBakeStatus = L"No walkable triangles passed bake filters.";
		scene->SetNavigationTriangles({});
		CEditor::GetInstance().ClearNavigationPreviewTriangles();
		CDebug::LogWarnning(
			L"[Navigation] Bake skipped: no walkable triangles. Rejected by slope: " +
			to_wstring(rejectedSlopeTriangleCount) +
			L", Passed by step height: " + to_wstring(passedStepHeightSlopeTriangleCount) +
			L", Rejected by headroom: " + to_wstring(rejectedHeadroomTriangleCount) +
			L", Rejected by obstacle radius: " + to_wstring(rejectedObstacleTriangleCount) +
			L", Rejected by step height: " + to_wstring(rejectedStepTriangleCount));
		return;
	}

	while (true)
	{
		struct EdgeOwner
		{
			size_t polygonIndex = 0;
			size_t edgeIndex = 0;
		};

		unordered_map<QuantizedEdgeKey, vector<EdgeOwner>, QuantizedEdgeKeyHash> edgeOwners = {};
		edgeOwners.reserve(bakedPolygons.size() * 4);
		for (size_t polygonIndex = 0; polygonIndex < bakedPolygons.size(); ++polygonIndex)
		{
			const NavBakePolygon& polygon = bakedPolygons[polygonIndex];
			for (size_t edgeIndex = 0; edgeIndex < polygon.vertices.size(); ++edgeIndex)
			{
				const size_t nextIndex = (edgeIndex + 1) % polygon.vertices.size();
				edgeOwners[MakeEdgeKey(polygon.vertices[edgeIndex], polygon.vertices[nextIndex])].push_back({ polygonIndex, edgeIndex });
			}
		}

		_bool mergedPolygon = false;
		for (auto& edgeEntry : edgeOwners)
		{
			if (edgeEntry.second.size() != 2)
				continue;

			const EdgeOwner ownerA = edgeEntry.second[0];
			const EdgeOwner ownerB = edgeEntry.second[1];
			if (ownerA.polygonIndex == ownerB.polygonIndex)
				continue;

			const size_t keepIndex = min(ownerA.polygonIndex, ownerB.polygonIndex);
			const size_t removeIndex = max(ownerA.polygonIndex, ownerB.polygonIndex);
			const EdgeOwner keepOwner = (ownerA.polygonIndex == keepIndex) ? ownerA : ownerB;
			const EdgeOwner removeOwner = (ownerA.polygonIndex == removeIndex) ? ownerA : ownerB;

			NavBakePolygon merged = {};
			if (!TryMergePolygons(bakedPolygons[keepIndex], keepOwner.edgeIndex, bakedPolygons[removeIndex], removeOwner.edgeIndex, merged))
				continue;

			bakedPolygons[keepIndex] = move(merged);
			bakedPolygons.erase(bakedPolygons.begin() + removeIndex);
			++mergedEdgeCount;
			mergedPolygon = true;
			break;
		}

		if (!mergedPolygon)
			break;
	}

	vector<vector3> previewTriangles = {};
	previewTriangles.reserve(sourceTriangleCount * 3);
	for (const NavBakePolygon& polygon : bakedPolygons)
	{
		if (polygon.vertices.size() < 3)
			continue;

		for (size_t i = 1; i + 1 < polygon.vertices.size(); ++i)
		{
			previewTriangles.push_back(polygon.vertices[0]);
			previewTriangles.push_back(polygon.vertices[i]);
			previewTriangles.push_back(polygon.vertices[i + 1]);
		}
	}

	if (previewTriangles.empty())
	{
		m_strNavigationBakeStatus = L"No merged navigation polygons were generated.";
		scene->SetNavigationTriangles({});
		CEditor::GetInstance().ClearNavigationPreviewTriangles();
		CDebug::LogWarnning(L"[Navigation] Bake skipped: polygon merge produced no renderable preview.");
		return;
	}

	scene->SetNavigationTriangles(previewTriangles);
	CEditor::GetInstance().SetNavigationPreviewTriangles(previewTriangles);
	CEditor::GetInstance().SetNavigationGizmoVisible(true);
	m_strNavigationBakeStatus =
		L"Baked " + to_wstring(static_cast<_int>(previewTriangles.size() / 3)) + L" preview triangles as " +
		to_wstring(static_cast<_int>(bakedPolygons.size())) + L" merged polygons";
	CDebug::Log(
		L"[Navigation] Bake preview generated. Objects: " + to_wstring(sourceObjectCount) +
		L", Meshes: " + to_wstring(sourceMeshCount) +
		L", Walkable Triangles: " + to_wstring(sourceTriangleCount) +
		L", Preview Triangles: " + to_wstring(static_cast<_int>(previewTriangles.size() / 3)) +
		L", Merged Polygons: " + to_wstring(static_cast<_int>(bakedPolygons.size())) +
		L", Removed Internal Edges: " + to_wstring(mergedEdgeCount) +
		L", Rejected By Slope: " + to_wstring(rejectedSlopeTriangleCount) +
		L", Passed By Step Height: " + to_wstring(passedStepHeightSlopeTriangleCount) +
		L", Rejected By Headroom: " + to_wstring(rejectedHeadroomTriangleCount) +
		L", Rejected By Obstacle Radius: " + to_wstring(rejectedObstacleTriangleCount) +
		L", Rejected By Step Height: " + to_wstring(rejectedStepTriangleCount));
}
void CTopToolBar::CollectNavigationBakeStats(_int& outObjectCount, _int& outMeshCount, _int& outTriangleCount) const
{
	outObjectCount = 0;
	outMeshCount = 0;
	outTriangleCount = 0;
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!scene)
		return;
	for (CGameObject* obj : scene->Get_ObjectList())
	{
		if (!obj || !obj->Is_SaveTarget())
			continue;
		if (m_bNavigationStaticOnly && !obj->IsStatic(CGameObject::STATIC_METHOD::NavigationStatic))
			continue;
		CMeshRenderer* meshRenderer = obj->GetComponent<CMeshRenderer>();
		if (!meshRenderer)
			continue;
		CMeshBuffer* meshBuffer = meshRenderer->Get_MeshBuffer();
		if (!meshBuffer)
			continue;
		++outObjectCount;
		++outMeshCount;
		outTriangleCount += static_cast<_int>(meshBuffer->Get_Info().indexCount / 3u);
	}
}




