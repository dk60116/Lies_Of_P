#include "epch.h"
#include "SceneManager.h"
#include "RenderTargetManager.h"

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
}

CSceneManager::CSceneManager()
	: m_pCrtScene(nullptr)
	, m_pTempScene(nullptr)
	, m_mSceneList({})
	, m_bLoading(false)
	, m_bSceneAwakened(false)
	, m_ePlayState(PlayState::Stopped)
	, m_strPlayStartSceneName(L"")
	, m_bStepFrameRequested(false)
	, m_pEditorCamObj(nullptr)
	, m_pEditorCamera(nullptr)
	, m_sTimeSetting({})
	, m_sLightSetting({})
	, m_mLayerList()
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

	for (_uint i = 1; i < 32; ++i)
		m_mLayerList.insert({ 1u << (_uint)i, L"Layer_" +  to_wstring(i)});

	LoadLayerSettings();
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
	CScene* newScene = dynamic_cast<CScene*>(_newScene);
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

#ifndef  _CLIENT_BUILD
	if (m_pCrtScene)
		CEditor::GetInstance().Set_EditorCamTransform(Get_EditorCamera()->Get_Transform());
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
		m_strPlayStartSceneName = m_pCrtScene->Get_SceneName();

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

	const wstring stopTargetSceneName = m_strPlayStartSceneName.empty() ? m_pCrtScene->Get_SceneName() : m_strPlayStartSceneName;
	const _bool isSameSceneReload = stopTargetSceneName == m_pCrtScene->Get_SceneName();

	m_ePlayState = PlayState::Stopped;
	m_bSceneAwakened = false;
	m_strPlayStartSceneName = L"";
	m_bStepFrameRequested = false;

	if (isSameSceneReload)
	{
		wstring file = m_pCrtScene->Get_SceneName() + L".scenedata";
		auto sceneTransformInfo = CResources::GetInstance().ReadSceneObjectTransformInfos(file);
		m_pCrtScene->Bind_ObjectsTransform(sceneTransformInfo);
		return;
	}

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
	m_mLayerList[mask] = _name;
	SaveEngineSettings();
}

const map<_uint, wstring>& CSceneManager::Get_LayerList() const
{
	return m_mLayerList;
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
	outFile << "LightShadowQuality=" << static_cast<_int>(m_sLightSetting.shadowQuality) << "\n";

	for (_uint i = 1u; i < 32u; ++i)
	{
		const _uint mask = (1u << i);
		auto it = m_mLayerList.find(mask);
		if (it == m_mLayerList.end())
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

		try
		{
			unsigned long idx = stoul(idxText);
			if (idx < 1u || idx >= 32u)
				continue;

			m_mLayerList[1u << static_cast<_uint>(idx)] = CEngineString::StringToWString(nameText);
		}
		catch (...)
		{
		}
	}
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

	constexpr wchar_t kPrefix[] = L"Layer_";
	if (_name.rfind(kPrefix, 0u) == 0u)
	{
		const wstring idxStr = _name.substr(std::size(kPrefix) - 1);
		if (!idxStr.empty())
		{
			try
			{
				const unsigned long idx = stoul(idxStr);
				if (idx < 32u)
					return (1u << idx);
			}
			catch (...) { }
		}
	}

	return 0u;
}

const wstring& CSceneManager::LayerToName(_uint _index)
{
	static const wstring kEmpty = L"";

	auto it = m_mLayerList.find(_index);
	if (it != m_mLayerList.end())
		return it->second;

	if (_index < 32)
	{
		const _uint mask = (1u << _index);
		it = m_mLayerList.find(mask);
		if (it != m_mLayerList.end())
			return it->second;
	}

	return kEmpty;
}

const CSceneManager::LayerMask CSceneManager::MakeLayerMask(const vector<_uint> _layers) const
{
	LayerMask mask = 0;
	for (_uint idx : _layers)
	{
		if (idx < 32)
			mask |= (LayerMask(1u) << idx);
	}
	return mask;
}

const _bool CSceneManager::ContainLayerMask(const _uint _layer, const LayerMask _mask)
{
	return (_layer & _mask) != 0;
}
