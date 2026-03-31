#include "epch.h"
#include "TopToolBar.h"

#include "Editor.h"
#include "Physics.h"
#include "MeshFilter.h"
#include "MeshRenderer.h"
#include "SkinnedMeshRenderer.h"

namespace
{
	string BuildDefaultNavigationResourceName(CScene* scene)
	{
		if (!scene)
			return "NavigationMesh";

		string sceneName = CEngineString::WStringToString(scene->Get_SceneName());
		if (sceneName.empty())
			sceneName = "Scene";

		return sceneName + " (NavigationMesh)";
	}

	CMeshBuffer* ResolveNavigationMeshBuffer(CGameObject* object)
	{
		if (!object)
			return nullptr;

		if (CMeshRenderer* meshRenderer = object->GetComponent<CMeshRenderer>())
			return meshRenderer->Get_MeshBuffer();

		if (CSkinnedMeshRenderer* skinnedMeshRenderer = object->GetComponent<CSkinnedMeshRenderer>())
			return skinnedMeshRenderer->Get_MeshBuffer();

		if (CMeshFilter* meshFilter = object->GetComponent<CMeshFilter>())
			return meshFilter->Get_MeshBuffer();

		return nullptr;
	}

	vector<EngineAI::CNaviMesh::MeshSource> GatherNavigationStaticMeshSources(CScene* scene)
	{
		vector<EngineAI::CNaviMesh::MeshSource> sources = {};
		if (!scene)
			return sources;

		for (CGameObject* object : scene->Get_ObjectList())
		{
			if (!object || !object->IsActive())
				continue;

			const _bool isNavigationStatic = object->IsStatic(CGameObject::STATIC_METHOD::NavigationStatic);
			const _bool isNavigationObstacle = object->IsStatic(CGameObject::STATIC_METHOD::NavigationObstacle);
			if (!isNavigationStatic && !isNavigationObstacle)
				continue;

			CMeshBuffer* meshBuffer = ResolveNavigationMeshBuffer(object);
			if (!meshBuffer)
				continue;

			EngineAI::CNaviMesh::MeshSource source = {};
			source.meshBuffer = meshBuffer;
			source.label = object->Get_ObjectNameID();
			source.walkable = isNavigationStatic && !isNavigationObstacle;
			_matrix worldMatrix = XMMatrixIdentity();
			if (CTransform* transform = object->GetTransform())
				worldMatrix = transform->Get_WorldMatrix();
			XMStoreFloat4x4(&source.worldMatrix, worldMatrix);
			sources.push_back(source);
		}

		return sources;
	}

	bool BuildNavigationMeshResource(
		CScene* scene,
		const wstring& resourceName,
		const vector<EngineAI::CNaviMesh::MeshSource>& meshSources,
		const EngineAI::CNaviMesh::NavBakeOptions& bakeOptions,
		string& outStatus,
		int& outPolygonCount)
	{
		outPolygonCount = 0;

		if (!scene)
		{
			outStatus = "No active scene.";
			return false;
		}

		if (meshSources.empty())
		{
			outStatus = "No NavigationStatic or NavigationObstacle meshes were found in the current scene.";
			return false;
		}

		_bool hasWalkableSource = false;
		for (const auto& meshSource : meshSources)
		{
			if (meshSource.walkable)
			{
				hasWalkableSource = true;
				break;
			}
		}

		if (!hasWalkableSource)
		{
			outStatus = "No walkable NavigationStatic meshes were found in the current scene.";
			return false;
		}

		if (resourceName.empty())
		{
			outStatus = "Enter a NavigationMesh resource name.";
			return false;
		}

		CEngineResource* existingResource = scene->Find_Resource(resourceName);
		EngineAI::CNaviMesh* navMesh = nullptr;
		_bool createdNew = false;

		if (existingResource)
		{
			navMesh = dynamic_cast<EngineAI::CNaviMesh*>(existingResource);
			if (!navMesh)
			{
				outStatus = "The resource name is already used by another resource type.";
				return false;
			}
		}
		else
		{
			navMesh = EngineAI::CNaviMesh::CreateRuntime(resourceName);
			if (!navMesh)
			{
				outStatus = "Failed to create the NavigationMesh resource.";
				return false;
			}

			createdNew = true;
		}

		navMesh->SetBakeOptions(bakeOptions);
		if (FAILED(navMesh->BuildFromSources(meshSources)))
		{
			if (createdNew)
				Safe_Release(navMesh);

			outStatus = "NavigationMesh build failed. Check the debug log for details.";
			return false;
		}

		if (createdNew)
		{
			if (!scene->Add_Resource(resourceName, navMesh))
			{
				Safe_Release(navMesh);
				outStatus = "Build succeeded, but scene registration failed.";
				return false;
			}

			Safe_Release(navMesh);
		}

		outPolygonCount = static_cast<int>(navMesh->GetPolygons().size());
		outStatus = "NavigationMesh built successfully.";
		return true;
	}

	bool ClearNavigationMeshResource(CScene* scene, const wstring& resourceName, string& outStatus)
	{
		if (!scene)
		{
			outStatus = "No active scene.";
			return false;
		}

		if (resourceName.empty())
		{
			outStatus = "Enter a NavigationMesh resource name.";
			return false;
		}

		CEngineResource* existingResource = scene->Find_Resource(resourceName);
		if (!existingResource)
		{
			outStatus = "No NavigationMesh resource with that name exists in the scene.";
			return false;
		}

		if (!dynamic_cast<EngineAI::CNaviMesh*>(existingResource))
		{
			outStatus = "The resource name is already used by another resource type.";
			return false;
		}

		if (!scene->Remove_Resource(resourceName))
		{
			outStatus = "NavigationMesh clear failed.";
			return false;
		}

		outStatus = "NavigationMesh cleared.";
		return true;
	}
}

CTopToolBar::CTopToolBar()
	: m_bProjectSettingsWindowOpen(false)
	, m_iProjectSettingsSelection(0)
	, m_fPendingFixedTimeStep(0.02f)
	, m_fPendingTimeScale(1.f)
	, m_vPendingGravity(0.f, -9.81f, 0.f)
	, m_iPendingShadowQuality(0)
	, m_bSceneSettingsWindowOpen(false)
	, m_bNavigationWindowOpen(false)
	, m_bNavigationBuildSucceeded(false)
	, m_iNavigationPolygonCount(0)
{
	m_arrPendingPhysicsLayerCollisionMasks.fill(~0u);
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
	ShowViewMenu();
	ShowAIMenu();
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
			auto physicsSetting = sceneManager.Get_PhysicsSetting();
			auto lightSetting = sceneManager.Get_LightSetting();
			m_fPendingFixedTimeStep = timeSetting.fixedTimeStep;
			m_fPendingTimeScale = timeSetting.timeSclae;
			m_vPendingGravity = physicsSetting.gravity;
			m_arrPendingPhysicsLayerCollisionMasks = physicsSetting.layerCollisionMatrix;
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

	CEditor& editor = CEditor::GetInstance();

	if (ImGui::Button("View"))
		ImGui::OpenPopup("ViewMenuPopup");

	if (ImGui::BeginPopup("ViewMenuPopup"))
	{
		_bool showCollider = editor.IsColliderGizmoVisible();
		if (ImGui::MenuItem("Collider", nullptr, showCollider))
			editor.SetColliderGizmoVisible(!showCollider);

		_bool showMeshCollider = editor.IsMeshColliderGizmoVisible();
		if (ImGui::MenuItem("Mesh Collider", nullptr, showMeshCollider))
			editor.SetMeshColliderGizmoVisible(!showMeshCollider);

		_bool showNavigationMesh = editor.IsNavigationMeshVisible();
		if (ImGui::MenuItem("NaviMesh", nullptr, showNavigationMesh))
			editor.SetNavigationMeshVisible(!showNavigationMesh);

		_bool showGameStatus = editor.IsGameStatusWindowVisible();
		if (ImGui::MenuItem("Game Status", nullptr, showGameStatus))
			editor.SetGameStatusWindowVisible(!showGameStatus);

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
		{
			m_bNavigationWindowOpen = true;
			if (m_strNavigationResourceName.empty())
				m_strNavigationResourceName = BuildDefaultNavigationResourceName(CSceneManager::GetInstance().Get_CrtScene());
		}

		ImGui::EndPopup();
	}
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
		_float softShadowLightSize = setting.softShadowLightSize;

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

		if (ImGui::DragFloat("Soft Shadow Light Size", &softShadowLightSize, 0.1f, 0.f, 50.f, "%.1f"))
			currentScene->Set_SoftShadowLightSize(softShadowLightSize);

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

	ImGui::SetNextWindowSize(ImVec2(900.f, 560.f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Project Settings", &m_bProjectSettingsWindowOpen))
	{
		const _float buttonAreaHeight = ImGui::GetFrameHeightWithSpacing() + 6.f;
		ImGui::BeginChild("ProjectSettingsContent", ImVec2(0.f, -buttonAreaHeight), false);
		ImGui::BeginChild("ProjectSettingsCategoryList", ImVec2(180.f, 0.f), true);
		if (ImGui::Selectable("Time", m_iProjectSettingsSelection == 0))
			m_iProjectSettingsSelection = 0;
		if (ImGui::Selectable("Physics", m_iProjectSettingsSelection == 1))
			m_iProjectSettingsSelection = 1;
		if (ImGui::Selectable("Light", m_iProjectSettingsSelection == 2))
			m_iProjectSettingsSelection = 2;
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("ProjectSettingsEditor", ImVec2(0.f, 0.f), true);
		if (m_iProjectSettingsSelection == 0)
			ShowProjectSettingsTime();
		else if (m_iProjectSettingsSelection == 1)
			ShowProjectSettingsPhysics();
		else if (m_iProjectSettingsSelection == 2)
			ShowProjectSettingsLight();
		ImGui::EndChild();
		ImGui::EndChild();

		if (ImGui::Button("Save"))
		{
			CSceneManager& sceneManager = CSceneManager::GetInstance();
			CSceneManager::PhysicsSettings physicsSetting = sceneManager.Get_PhysicsSetting();
			physicsSetting.gravity = m_vPendingGravity;
			physicsSetting.layerCollisionMatrix = m_arrPendingPhysicsLayerCollisionMasks;
			sceneManager.Set_FixedTimeStep(m_fPendingFixedTimeStep);
			sceneManager.Set_TimeScale(m_fPendingTimeScale);
			sceneManager.Set_PhysicsSettings(physicsSetting);
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

void CTopToolBar::ShowProjectSettingsPhysics()
{
	ImGui::Text("Physics");
	ImGui::Separator();

	ImGui::DragFloat3("Gravity", &m_vPendingGravity.x, 0.1f, -1000.f, 1000.f, "%.2f");
	ImGui::Spacing();
	ImGui::Text("Layer Collision Matrix");

	ImGui::BeginChild("PhysicsLayerCollisionMatrix", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_HorizontalScrollbar);

	vector<_uint> visibleLayerIndices = { 0u };
	CSceneManager& sceneManager = CSceneManager::GetInstance();
	const auto& layerList = sceneManager.Get_LayerList();
	auto getLayerDisplayName = [&sceneManager](_uint layerIndex)
	{
		const _uint layerMask = layerIndex == 0u ? 0u : (1u << layerIndex);
		string layerName = CEngineString::WStringToString(sceneManager.LayerToName(layerMask));
		if (layerName.empty())
			layerName = layerIndex == 0u ? "Default" : "<Empty>";
		return layerName;
	};
	for (_uint layerIndex = 1u; layerIndex < CPhysics::MAX_SCENE_LAYERS; ++layerIndex)
	{
		const _uint layerMask = (1u << layerIndex);
		auto it = layerList.find(layerMask);
		if (it == layerList.end())
			continue;
		if (CEngineString::Trim(it->second).empty())
			continue;
		visibleLayerIndices.push_back(layerIndex);
	}

	const ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("PhysicsLayerCollisionTable", static_cast<_int>(visibleLayerIndices.size() + 1u), tableFlags))
	{
		ImGui::TableSetupScrollFreeze(1, 1);
		ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 180.f);
		for (const _uint columnIndex : visibleLayerIndices)
		{
			const string columnName = getLayerDisplayName(columnIndex);
			ImGui::TableSetupColumn(columnName.c_str(), ImGuiTableColumnFlags_WidthFixed, 120.f);
		}
		ImGui::TableHeadersRow();

		for (const _uint rowIndex : visibleLayerIndices)
		{
			const string rowName = getLayerDisplayName(rowIndex);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(rowName.c_str());

			for (_uint columnSlot = 0u; columnSlot < visibleLayerIndices.size(); ++columnSlot)
			{
				const _uint columnIndex = visibleLayerIndices[columnSlot];
				const _uint bit = (_uint(1u) << columnIndex);
				_bool enabled = (m_arrPendingPhysicsLayerCollisionMasks[rowIndex] & bit) != 0u;

				ImGui::TableSetColumnIndex(static_cast<_int>(columnSlot + 1u));
				ImGui::PushID(static_cast<_int>(rowIndex * CPhysics::MAX_SCENE_LAYERS + columnIndex));
				if (ImGui::Checkbox("##CollisionEnabled", &enabled))
				{
					const _uint symmetricBit = (_uint(1u) << rowIndex);
					if (enabled)
					{
						m_arrPendingPhysicsLayerCollisionMasks[rowIndex] |= bit;
						m_arrPendingPhysicsLayerCollisionMasks[columnIndex] |= symmetricBit;
					}
					else
					{
						m_arrPendingPhysicsLayerCollisionMasks[rowIndex] &= ~bit;
						m_arrPendingPhysicsLayerCollisionMasks[columnIndex] &= ~symmetricBit;
					}
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
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
		editorCam->GetTransform()->Set_PositionZ(-999999.f);
		editorCam->GetTransform()->Set_EulerAngles(vector3::zero());
		editorCam->SetViewMode(CCamera::ViewMode::Orthographic);
	}
}

void CTopToolBar::ShowGameStatusWindow()
{
	CEditor& editor = CEditor::GetInstance();
	_bool isOpen = editor.IsGameStatusWindowVisible();
	if (!isOpen)
		return;

	ImGui::SetNextWindowSize(ImVec2(320.f, 180.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Game Status", &isOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		if (isOpen != editor.IsGameStatusWindowVisible())
			editor.SetGameStatusWindowVisible(isOpen);
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
		if (isOpen != editor.IsGameStatusWindowVisible())
			editor.SetGameStatusWindowVisible(isOpen);
		return;
	}

	const CCamera::RenderStats& stats = camera->GetRenderStats();
	ImGui::Text("FPS: %d", CTime::GetInstance().Get_FPS());
	ImGui::Text("Batches: %u", stats.batches);
	ImGui::Text("Tris: %u", stats.tris);
	ImGui::Text("Verts: %u", stats.verts);
	ImGui::Text("UIImageBatches: %u", stats.uiImageBatches);
	ImGui::Text("UIImageInstances: %u", stats.uiImageInstances);
	ImGui::Text("Screen: %u x %u", screenWidth, screenHeight);
	ImGui::Text("VisibleSkinnedMeshes: %u", stats.visibleSkinnedMeshes);

	ImGui::End();

	if (isOpen != editor.IsGameStatusWindowVisible())
		editor.SetGameStatusWindowVisible(isOpen);
}

void CTopToolBar::ShowNavigationWindow()
{
	if (!m_bNavigationWindowOpen)
		return;

	ImGui::SetNextWindowSize(ImVec2(460.f, 520.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Navigation", &m_bNavigationWindowOpen, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}

	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (m_strNavigationResourceName.empty())
		m_strNavigationResourceName = BuildDefaultNavigationResourceName(scene);

	const vector<EngineAI::CNaviMesh::MeshSource> meshSources = GatherNavigationStaticMeshSources(scene);
	_int walkableMeshCount = 0;
	_int obstacleMeshCount = 0;
	for (const auto& meshSource : meshSources)
	{
		if (meshSource.walkable)
			++walkableMeshCount;
		else
			++obstacleMeshCount;
	}
	const string sceneName = scene ? CEngineString::WStringToString(scene->Get_SceneName()) : "None";

	ImGui::Text("Scene: %s", sceneName.c_str());
	ImGui::Separator();
	ImGui::Text("Walkable Meshes: %d", walkableMeshCount);
	ImGui::Text("Obstacle Meshes: %d", obstacleMeshCount);
	ImGui::TextWrapped("Use NavigationStatic for walkable surfaces and NavigationObstacle for non-walkable carve/obstacle meshes.");
	ImGui::Spacing();

	ImGui::InputText("Resource Name", &m_strNavigationResourceName);
	ImGui::SameLine();
	if (ImGui::Button("Use Scene Name"))
		m_strNavigationResourceName = BuildDefaultNavigationResourceName(scene);

	if (ImGui::CollapsingHeader("Bake Options", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Cell Size", &m_sNavigationBakeOptions.cellSize, 0.01f, 0.01f, 10.f, "%.2f");
		ImGui::DragFloat("Cell Height", &m_sNavigationBakeOptions.cellHeight, 0.01f, 0.01f, 10.f, "%.2f");
		ImGui::DragFloat("Agent Height", &m_sNavigationBakeOptions.agentHeight, 0.05f, 0.1f, 20.f, "%.2f");
		ImGui::DragFloat("Agent Radius", &m_sNavigationBakeOptions.agentRadius, 0.05f, 0.f, 10.f, "%.2f");
		ImGui::DragFloat("Step Height (Agent Max Climb)", &m_sNavigationBakeOptions.agentMaxClimb, 0.05f, 0.f, 10.f, "%.2f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Maximum step or ledge height the agent can traverse. This is converted to Recast walkableClimb using Cell Height.");
		ImGui::DragFloat("Agent Max Slope", &m_sNavigationBakeOptions.agentMaxSlope, 0.5f, 0.f, 89.9f, "%.1f");
		ImGui::DragInt("Region Min Size", &m_sNavigationBakeOptions.regionMinSize, 1.f, 0, 256);
		ImGui::DragInt("Region Merge Size", &m_sNavigationBakeOptions.regionMergeSize, 1.f, 0, 256);
		ImGui::DragFloat("Edge Max Len", &m_sNavigationBakeOptions.edgeMaxLen, 0.1f, 0.f, 256.f, "%.2f");
		ImGui::DragFloat("Edge Max Error", &m_sNavigationBakeOptions.edgeMaxError, 0.05f, 0.1f, 10.f, "%.2f");
		ImGui::DragInt("Verts Per Poly", &m_sNavigationBakeOptions.vertsPerPoly, 1.f, 3, DT_VERTS_PER_POLYGON);
		ImGui::DragFloat("Detail Sample Dist", &m_sNavigationBakeOptions.detailSampleDist, 0.1f, 0.f, 32.f, "%.2f");
		ImGui::DragFloat("Detail Sample Max Error", &m_sNavigationBakeOptions.detailSampleMaxError, 0.05f, 0.f, 16.f, "%.2f");
		ImGui::DragFloat3("Query Half Extents", &m_sNavigationBakeOptions.queryHalfExtents.x, 0.1f, 0.1f, 100.f, "%.2f");
	}

	const _bool canBuild = scene != nullptr && walkableMeshCount > 0 && !m_strNavigationResourceName.empty();
	const _bool canClear = scene != nullptr && !m_strNavigationResourceName.empty();
	const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
	const float buttonWidth = (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5f;

	if (!canBuild)
		ImGui::BeginDisabled();

	if (ImGui::Button("Build NavigationMesh", ImVec2(buttonWidth, 0.f)))
	{
		const wstring resourceName = CEngineString::StringToWString(m_strNavigationResourceName);
		m_bNavigationBuildSucceeded = BuildNavigationMeshResource(
			scene,
			resourceName,
			meshSources,
			m_sNavigationBakeOptions,
			m_strNavigationBuildStatus,
			m_iNavigationPolygonCount);
	}

	if (!canBuild)
		ImGui::EndDisabled();

	ImGui::SameLine();

	if (!canClear)
		ImGui::BeginDisabled();

	if (ImGui::Button("Clear", ImVec2(buttonWidth, 0.f)))
	{
		const wstring resourceName = CEngineString::StringToWString(m_strNavigationResourceName);
		m_bNavigationBuildSucceeded = ClearNavigationMeshResource(scene, resourceName, m_strNavigationBuildStatus);
		if (m_bNavigationBuildSucceeded)
			m_iNavigationPolygonCount = 0;
	}

	if (!canClear)
		ImGui::EndDisabled();

	if (!scene)
		ImGui::TextUnformatted("No active scene.");
	else if (meshSources.empty())
		ImGui::TextUnformatted("No active NavigationStatic or NavigationObstacle meshes were found in the current scene.");
	else if (walkableMeshCount == 0)
		ImGui::TextUnformatted("At least one walkable NavigationStatic mesh is required to build a NavigationMesh.");
	else if (m_strNavigationResourceName.empty())
		ImGui::TextUnformatted("Enter a resource name before building.");

	if (!m_strNavigationBuildStatus.empty())
	{
		const ImVec4 color = m_bNavigationBuildSucceeded
			? ImVec4(0.25f, 0.85f, 0.35f, 1.f)
			: ImVec4(0.95f, 0.35f, 0.35f, 1.f);
		ImGui::Spacing();
		ImGui::TextColored(color, "%s", m_strNavigationBuildStatus.c_str());
		if (m_bNavigationBuildSucceeded)
			ImGui::Text("Polygons: %d", m_iNavigationPolygonCount);
	}

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

	ImVec2 textSize = ImGui::CalcTextSize(fpsText);

	_float rightMargin = 8.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textSize.x - rightMargin);

	ImGui::SameLine();
	_float textWidth = ImGui::CalcTextSize(fpsText).x;
	_float availableWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - textWidth);
	ImGui::TextUnformatted(fpsText);
}
