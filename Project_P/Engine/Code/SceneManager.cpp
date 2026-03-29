#include "epch.h"
#include "SceneManager.h"
#include "RenderTargetManager.h"
#include "Physics.h"
#include "JobSystem.h"

namespace
{
	static fs::path ResolveEngineSettingsPath(const _bool forSave)
	{
		const fs::path candidates[] =
		{
			"../Engine/Default/EngineSettings.setting",
			"Engine/Default/EngineSettings.setting",
			"../Project_P/Engine/Default/EngineSettings.setting",
			"Project_P/Engine/Default/EngineSettings.setting"
		};

		for (const fs::path& path : candidates)
		{
			if (fs::exists(path))
				return path;
		}

		if (forSave)
			return candidates[3];

		return candidates[0];
	}

	static constexpr _uint kPhysicsLayerCount = CPhysics::MAX_SCENE_LAYERS;

	static _bool GetPhysicsLayerCollisionBit(const CSceneManager::PhysicsSettings& settings, const _uint row, const _uint column)
	{
		if (row >= kPhysicsLayerCount || column >= kPhysicsLayerCount)
			return false;

		const CSceneManager::LayerMask bit = (CSceneManager::LayerMask(1u) << column);
		return (settings.layerCollisionMatrix[row] & bit) != 0u;
	}

	static void SetPhysicsLayerCollisionBit(CSceneManager::PhysicsSettings& settings, const _uint row, const _uint column, const _bool enabled)
	{
		if (row >= kPhysicsLayerCount || column >= kPhysicsLayerCount)
			return;

		const CSceneManager::LayerMask bit = (CSceneManager::LayerMask(1u) << column);
		if (enabled)
			settings.layerCollisionMatrix[row] |= bit;
		else
			settings.layerCollisionMatrix[row] &= ~bit;
	}

	static CSceneManager::PhysicsSettings NormalizePhysicsSettings(CSceneManager::PhysicsSettings settings)
	{
		for (_uint row = 0u; row < kPhysicsLayerCount; ++row)
		{
			CSceneManager::LayerMask sanitized = 0u;
			for (_uint column = 0u; column < kPhysicsLayerCount; ++column)
			{
				if (GetPhysicsLayerCollisionBit(settings, row, column))
					sanitized |= (CSceneManager::LayerMask(1u) << column);
			}
			settings.layerCollisionMatrix[row] = sanitized;
		}

		for (_uint row = 0u; row < kPhysicsLayerCount; ++row)
		{
			for (_uint column = row + 1u; column < kPhysicsLayerCount; ++column)
			{
				const _bool enabled = GetPhysicsLayerCollisionBit(settings, row, column) || GetPhysicsLayerCollisionBit(settings, column, row);
				SetPhysicsLayerCollisionBit(settings, row, column, enabled);
				SetPhysicsLayerCollisionBit(settings, column, row, enabled);
			}
		}

		return settings;
	}
}

CSceneManager::CSceneManager()
	: m_pCrtScene(nullptr)
	, m_pTempScene(nullptr)
	, m_mSceneList({})
	, m_bLoading(false)
	, m_bSceneAwakened(false)
	, m_ePlayState(PlayState::Stopped)
	, m_strPlayStartSceneName(L"")
	, m_vPlayStartSceneTransforms()
	, m_iPlayStartObjectCount(0u)
	, m_bStepFrameRequested(false)
	, m_pEditorCamObj(nullptr)
	, m_pEditorCamera(nullptr)
	, m_sTimeSetting({})
	, m_sLightSetting({})
	, m_mLayerList()
	, m_vTagList()
{
}

CSceneManager::~CSceneManager()
{
	Release();
}

CSceneManager& CSceneManager::GetInstance()
{
	static CSceneManager inst;
	return inst;
}

HRESULT CSceneManager::Initialize()
{
	m_mLayerList.insert({ 0u, L"Default" });
	m_vTagList.push_back(L"Untagged");

	LoadLayerSettings();
	Set_PhysicsSettings(m_sPhysicsSetting);
	Set_ShadowQuality(m_sLightSetting.shadowQuality);
	CTime::GetInstance().SetTimeScale(m_sTimeSetting.timeSclae);

	return S_OK;
}

void CSceneManager::Release()
{
	m_pCrtScene = nullptr;
	m_bSceneAwakened = false;
	m_ePlayState = PlayState::Stopped;
	m_strPlayStartSceneName = L"";
	m_vPlayStartSceneTransforms.clear();
	m_iPlayStartObjectCount = 0u;
	m_bStepFrameRequested = false;

	for (TRAVERSAL_ITER(m_mSceneList, it))
	{
		(*it).second->SceneRelease();
		Safe_Release((*it).second);
	}

	m_mSceneList.clear();
}

CScene* CSceneManager::CreateScene(CScene* _newScene, const wstring& _sceneName)
{
	CScene* newScene = _newScene;
	if (!newScene)
	{
        CDebug::LogError(L"Create Scene Fail: Scene instance is null: " + _sceneName);
		return nullptr;
	}
	newScene->Set_SceneName(_sceneName);

	m_mSceneList.emplace(_sceneName, newScene);

	newScene->AddRef();

	string filePath = "../Assets/Scenes/" + CEngineString::WStringToString(_sceneName) + ".scene";

	if (!CResources::FileExists(filePath))
	{
		ofstream outFile(filePath);

		if (outFile.is_open())
		{
			outFile << "SceneName : " << CEngineString::WStringToString(_sceneName);
			outFile.close();
		}
	}

	return newScene;
}

CScene* CSceneManager::Get_CrtScene()
{
	return m_pCrtScene;
}

CScene* CSceneManager::Get_TempScene()
{
	return m_pTempScene;
}

const map<wstring, CScene*>& CSceneManager::Get_SceneList()
{
	return m_mSceneList;
}

const _bool CSceneManager::Is_Loading() const
{
	return m_bLoading;
}

void CSceneManager::LoadScene(wstring _scene)
{
	if (m_bLoading)
	{
		CDebug::LogError("Load Scene Fail: Already loading Scene.");
		return;
	}

	auto iter = m_mSceneList.find(_scene);

	if (iter == m_mSceneList.end())
	{
		CDebug::LogError(L"Load Scene Fail: Not found Scene: " + _scene);
		return;
	}
	else
		CDebug::Log(L"Load scene start: " + _scene);

	CPhysics::GetInstance().ClearRaycastDebugDisplay();

#ifndef  _CLIENT_BUILD
	if (m_pCrtScene)
		CEditor::GetInstance().Set_EditorCamTransform(Get_EditorCamera()->GetTransform());
#endif

	m_pTempScene = iter->second;

	if (m_pTempScene)
	{
		m_pTempScene->PreLoadResources();
		m_bLoading = true;
	}
}

void CSceneManager::LoadScene(CScene* _scene)
{
	LoadScene(_scene->Get_SceneName());
}

void CSceneManager::LoadComplete()
{
	CRenderThread::GetInstance().WaitIdle();
	if (m_pCrtScene)
		m_pCrtScene->CollectCompletedRenderFrames();

	CPhysics::GetInstance().ClearRaycastDebugDisplay();

	const PlayState prevPlayState = m_ePlayState;

	m_pCrtScene = nullptr;
	m_pCrtScene = m_pTempScene;
	m_pTempScene = nullptr;

	if (!m_pCrtScene)
	{
		m_bLoading = false;
		return;
	}

	m_pCrtScene->Set_SaveRegistrationEnabled(false);
	m_pCrtScene->Initialize();
	m_bLoading = false;
	m_bSceneAwakened = false;
	m_pCrtScene->Set_SaveRegistrationEnabled(true);

	wstring file = m_pCrtScene->Get_SceneName() + L".scenedata";
	auto sceneTransformInfo = CResources::GetInstance().ReadSceneObjectTransformInfos(file);
	m_pCrtScene->Bind_ObjectsTransform(sceneTransformInfo);

	wstring navFile = m_pCrtScene->Get_SceneName() + L".navdata";
	auto sceneNavigationInfo = CResources::GetInstance().ReadSceneNavigationInfos(navFile);
	m_pCrtScene->Bind_NavigationInfos(sceneNavigationInfo);

	if (prevPlayState != PlayState::Stopped)
	{
		m_pCrtScene->Set_SaveRegistrationEnabled(false);
		m_pCrtScene->Awake();
		m_pCrtScene->Set_SaveRegistrationEnabled(true);
		m_bSceneAwakened = true;
		m_ePlayState = prevPlayState;
	}
	else
	{
		m_ePlayState = PlayState::Stopped;
		m_strPlayStartSceneName = L"";
		m_bStepFrameRequested = false;
	}
}

void CSceneManager::PlayScene()
{
	if (!m_pCrtScene || m_bLoading)
		return;

	if (m_ePlayState == PlayState::Playing)
		return;

	if (m_ePlayState == PlayState::Stopped)
	{
		m_strPlayStartSceneName = m_pCrtScene->Get_SceneName();
		m_vPlayStartSceneTransforms = m_pCrtScene->Convert_ObjectsTransformInfo();
		m_iPlayStartObjectCount = m_vPlayStartSceneTransforms.size();
	}

	if (!m_bSceneAwakened)
	{
		m_pCrtScene->Set_SaveRegistrationEnabled(false);
		m_pCrtScene->Awake();
		m_pCrtScene->Set_SaveRegistrationEnabled(true);
		m_bSceneAwakened = true;
	}

	m_ePlayState = PlayState::Playing;
}

void CSceneManager::PauseScene()
{
	if (!m_pCrtScene || m_bLoading)
		return;

	if (m_ePlayState == PlayState::Playing)
		m_ePlayState = PlayState::Paused;
}

void CSceneManager::StopScene()
{
	if (!m_pCrtScene || m_bLoading)
		return;

	m_ePlayState = PlayState::Stopped;
	m_bSceneAwakened = false;
	m_bStepFrameRequested = false;

	if (!m_vPlayStartSceneTransforms.empty() && m_iPlayStartObjectCount == m_pCrtScene->Get_UniqueObjectCount())
	{
		m_pCrtScene->Bind_ObjectsTransform(m_vPlayStartSceneTransforms);
		m_vPlayStartSceneTransforms.clear();
		m_iPlayStartObjectCount = 0u;
		m_strPlayStartSceneName = L"";
		return;
	}

	const wstring stopTargetSceneName = m_strPlayStartSceneName.empty() ? m_pCrtScene->Get_SceneName() : m_strPlayStartSceneName;
	m_vPlayStartSceneTransforms.clear();
	m_iPlayStartObjectCount = 0u;
	m_strPlayStartSceneName = L"";
	LoadScene(stopTargetSceneName);
}


void CSceneManager::RequestStepFrame()
{
	if (!m_pCrtScene || m_bLoading)
		return;

	if (m_ePlayState != PlayState::Paused)
		return;

	m_bStepFrameRequested = true;
}

const _bool CSceneManager::ConsumeStepFrameRequest()
{
	const _bool requested = m_bStepFrameRequested;
	m_bStepFrameRequested = false;
	return requested;
}

const _bool CSceneManager::IsPlaying() const
{
	return m_ePlayState == PlayState::Playing;
}

const _bool CSceneManager::IsPaused() const
{
	return m_ePlayState == PlayState::Paused;
}

const _bool CSceneManager::IsPlayMode() const
{
	return m_ePlayState != PlayState::Stopped;
}

CCamera* CSceneManager::Get_EditorCamera()
{
	return m_pCrtScene->Get_EditorCamera();
}

const CSceneManager::TimeSettings& CSceneManager::Get_TimeSetting()
{
	return m_sTimeSetting;
}

const CSceneManager::PhysicsSettings& CSceneManager::Get_PhysicsSetting()
{
	return m_sPhysicsSetting;
}

const CSceneManager::LightSettings& CSceneManager::Get_LightSetting()
{
	return m_sLightSetting;
}

void CSceneManager::Set_FixedTimeStep(const _float value)
{
	_float newValue = value;
	if (newValue < 0.0001f)
		newValue = 0.0001f;
	m_sTimeSetting.fixedTimeStep = newValue;
}

void CSceneManager::Set_TimeScale(const _float value)
{
	_float newValue = value;
	if (newValue < 0.f)
		newValue = 0.f;
	m_sTimeSetting.timeSclae = newValue;
	CTime::GetInstance().SetTimeScale(m_sTimeSetting.timeSclae);
}

void CSceneManager::Set_PhysicsSettings(const PhysicsSettings& settings)
{
	m_sPhysicsSetting = NormalizePhysicsSettings(settings);
	CPhysics::GetInstance().SetGravity(m_sPhysicsSetting.gravity);
	CPhysics::GetInstance().SetLayerCollisionMasks(m_sPhysicsSetting.layerCollisionMatrix);
}

void CSceneManager::Set_ShadowQuality(const shadowQualityOptions option)
{
	m_sLightSetting.shadowQuality = option;

	switch (option)
	{
	case Low:
		m_sLightSetting.shadowMapSize = 1024;
		break;
	case Middle:
		m_sLightSetting.shadowMapSize = 2048;
		break;
	case High:
		m_sLightSetting.shadowMapSize = 4096;
		break;
	case SuperHigh:
		m_sLightSetting.shadowMapSize = 8192;
		break;
	case Ultra:
		m_sLightSetting.shadowMapSize = 16384;
		break;
	}

	if (IsPlayMode())
		CRenderTargetManager::GetInstance().RefreshShadowDepthTarget();
}

void CSceneManager::AddLayer(_uint _index, const wstring& _name)
{
	if (_index >= 32u)
		return;
	if (_index == 0u)
		return;

	const _uint mask = (1u << _index);
	const wstring trimmedName = CEngineString::Trim(_name);
	if (trimmedName.empty())
		m_mLayerList.erase(mask);
	else
		m_mLayerList[mask] = trimmedName;
	SaveEngineSettings();
}

const map<_uint, wstring>& CSceneManager::Get_LayerList() const
{
	return m_mLayerList;
}

const vector<wstring>& CSceneManager::Get_TagList() const
{
	return m_vTagList;
}

void CSceneManager::AddTag(const wstring& tag)
{
	const wstring trimmedTag = CEngineString::Trim(tag);
	if (trimmedTag.empty())
		return;

	for (const wstring& existingTag : m_vTagList)
	{
		if (existingTag == trimmedTag)
			return;
	}

	m_vTagList.push_back(trimmedTag);
	SaveEngineSettings();
}

void CSceneManager::RemoveTag(const wstring& tag)
{
	const wstring trimmedTag = CEngineString::Trim(tag);
	if (trimmedTag.empty() || trimmedTag == L"Untagged")
		return;

	vector<wstring>::iterator removeIt = m_vTagList.end();
	for (vector<wstring>::iterator it = m_vTagList.begin(); it != m_vTagList.end(); ++it)
	{
		if (*it == trimmedTag)
		{
			removeIt = it;
			break;
		}
	}
	if (removeIt == m_vTagList.end())
		return;

	m_vTagList.erase(removeIt);

	for (const auto& scenePair : m_mSceneList)
	{
		CScene* scene = scenePair.second;
		if (!scene)
			continue;

		for (CGameObject* object : scene->Get_ObjectList())
		{
			if (!object)
				continue;
			if (object->GetTag() == trimmedTag)
				object->SetTag(L"Untagged");
		}
	}

	SaveEngineSettings();
}

void CSceneManager::SaveLayerSettings() const
{
	SaveEngineSettings();
}

void CSceneManager::SaveEngineSettings() const
{
	const fs::path settingsPath = ResolveEngineSettingsPath(true);
	fs::create_directories(settingsPath.parent_path());
	ofstream outFile(settingsPath, ios::trunc);
	if (!outFile.is_open())
		return;

	outFile << "TimeFixedStep=" << m_sTimeSetting.fixedTimeStep << "\n";
	outFile << "TimeScale=" << m_sTimeSetting.timeSclae << "\n";
	outFile << "PhysicsGravity=" << m_sPhysicsSetting.gravity.x << "," << m_sPhysicsSetting.gravity.y << "," << m_sPhysicsSetting.gravity.z << "\n";
	for (_uint row = 0u; row < kPhysicsLayerCount; ++row)
		outFile << "PhysicsLayerCollision" << row << "=" << m_sPhysicsSetting.layerCollisionMatrix[row] << "\n";
	outFile << "LightShadowQuality=" << static_cast<_int>(m_sLightSetting.shadowQuality) << "\n";
	for (const wstring& tag : m_vTagList)
	{
		const wstring trimmedTag = CEngineString::Trim(tag);
		if (trimmedTag.empty() || trimmedTag == L"Untagged")
			continue;
		outFile << "Tag=" << CEngineString::WStringToString(trimmedTag) << "\n";
	}

	for (_uint i = 1u; i < 32u; ++i)
	{
		const _uint mask = (1u << i);
		auto it = m_mLayerList.find(mask);
		if (it == m_mLayerList.end())
			continue;
		if (CEngineString::Trim(it->second).empty())
			continue;

		outFile << i << "=" << CEngineString::WStringToString(it->second) << "\n";
	}
}

void CSceneManager::LoadLayerSettings()
{
	const fs::path settingsPath = ResolveEngineSettingsPath(false);
	ifstream inFile(settingsPath);
	if (!inFile.is_open())
		return;

	string line;
	while (getline(inFile, line))
	{
		line = CEngineString::Trim(line);
		if (line.empty())
			continue;

		size_t delim = line.find('=');
		if (delim == string::npos)
			continue;

		string idxText = CEngineString::Trim(line.substr(0, delim));
		string nameText = CEngineString::Trim(line.substr(delim + 1));
		if (idxText.empty() || nameText.empty())
			continue;

		if (idxText == "TimeFixedStep")
		{
			try
			{
				_float parsed = stof(nameText);
				if (parsed < 0.0001f)
					parsed = 0.0001f;
				m_sTimeSetting.fixedTimeStep = parsed;
			}
			catch (...)
			{
			}
			continue;
		}

		if (idxText == "TimeScale")
		{
			try
			{
				_float parsed = stof(nameText);
				if (parsed < 0.f)
					parsed = 0.f;
				m_sTimeSetting.timeSclae = parsed;
			}
			catch (...)
			{
			}
			continue;
		}

		if (idxText == "PhysicsGravity")
		{
			const size_t comma0 = nameText.find(',');
			const size_t comma1 = comma0 == string::npos ? string::npos : nameText.find(',', comma0 + 1u);
			if (comma0 != string::npos && comma1 != string::npos)
			{
				try
				{
					m_sPhysicsSetting.gravity.x = stof(CEngineString::Trim(nameText.substr(0, comma0)));
					m_sPhysicsSetting.gravity.y = stof(CEngineString::Trim(nameText.substr(comma0 + 1u, comma1 - comma0 - 1u)));
					m_sPhysicsSetting.gravity.z = stof(CEngineString::Trim(nameText.substr(comma1 + 1u)));
				}
				catch (...)
				{
				}
			}
			continue;
		}

		if (idxText.rfind("PhysicsLayerCollision", 0u) == 0u)
		{
			try
			{
				const unsigned long row = stoul(idxText.substr(strlen("PhysicsLayerCollision")));
				if (row < kPhysicsLayerCount)
					m_sPhysicsSetting.layerCollisionMatrix[static_cast<_uint>(row)] = static_cast<LayerMask>(stoul(nameText));
			}
			catch (...)
			{
			}
			continue;
		}

		if (idxText == "LightShadowQuality")
		{
			try
			{
				_int parsed = stoi(nameText);
				if (parsed < static_cast<_int>(CSceneManager::shadowQualityOptions::Low))
					parsed = static_cast<_int>(CSceneManager::shadowQualityOptions::Low);
				if (parsed > static_cast<_int>(CSceneManager::shadowQualityOptions::Ultra))
					parsed = static_cast<_int>(CSceneManager::shadowQualityOptions::Ultra);
				m_sLightSetting.shadowQuality = static_cast<CSceneManager::shadowQualityOptions>(parsed);
			}
			catch (...)
			{
			}
			continue;
		}

		if (idxText == "Tag")
		{
			const wstring trimmedTag = CEngineString::Trim(CEngineString::StringToWString(nameText));
			if (!trimmedTag.empty())
			{
				_bool exists = false;
				for (const wstring& existingTag : m_vTagList)
				{
					if (existingTag == trimmedTag)
					{
						exists = true;
						break;
					}
				}
				if (!exists)
					m_vTagList.push_back(trimmedTag);
			}
			continue;
		}

		try
		{
			unsigned long idx = stoul(idxText);
			if (idx < 1u || idx >= 32u)
				continue;

			const string defaultLayerName = "Layer_" + to_string(idx);
			if (nameText == defaultLayerName)
				continue;

			const wstring trimmedLayerName = CEngineString::Trim(CEngineString::StringToWString(nameText));
			if (trimmedLayerName.empty())
				continue;

			m_mLayerList[1u << static_cast<_uint>(idx)] = trimmedLayerName;
		}
		catch (...)
		{
		}
	}

	m_sPhysicsSetting = NormalizePhysicsSettings(m_sPhysicsSetting);
}

const _uint CSceneManager::NameToLayer(const wstring& _name) const
{
	if (_name.empty())
		return 0u;

	for (const auto& kv : m_mLayerList)
	{
		const _uint   mask = kv.first;
		const wstring& name = kv.second;

		if (name == _name)
			return mask;
	}

	return 0u;
}

const wstring& CSceneManager::LayerToName(_uint _index)
{
	static const wstring kEmpty = L"";

	auto it = m_mLayerList.find(_index);
	if (it != m_mLayerList.end())
		return it->second;

	if (_index < 32u)
	{
		const _uint mask = (1u << _index);
		it = m_mLayerList.find(mask);
		if (it != m_mLayerList.end())
			return it->second;
	}

	return kEmpty;
}

const CSceneManager::LayerMask CSceneManager::MakeLayerMask(const _bool _all, const vector<_uint> _layers) const
{
	auto toLayerMask = [](const _uint layer) -> LayerMask
	{
		if (layer == 0u)
			return LayerMask(1u);

		if ((layer & (layer - 1u)) == 0u)
			return static_cast<LayerMask>(layer);

		if (layer < 32u)
			return (LayerMask(1u) << layer);

		return 0u;
	};

	LayerMask mask = 0u;

	if (!_all)
	{
		for (const _uint layer : _layers)
		{
			mask |= toLayerMask(layer);
		}
		return mask;
	}

	mask = ~LayerMask(0u);

	for (const _uint layer : _layers)
	{
		const LayerMask layerMask = toLayerMask(layer);
		if (layerMask != 0u)
			mask &= ~layerMask;
	}

	return mask;
}

const _bool CSceneManager::ContainLayerMask(const _uint _layer, const LayerMask _mask)
{
	if (_layer == 0u)
		return (_mask & LayerMask(1u)) != 0u;

	return (_layer & _mask) != 0u;
}
