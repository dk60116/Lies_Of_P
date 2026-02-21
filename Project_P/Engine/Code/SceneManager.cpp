#include "epch.h"
#include "SceneManager.h"

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
	Set_ShadowQuality(m_sLightSetting.shadowQuality);

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

	const _bool sameSceneReload = (m_pCrtScene && m_pCrtScene->Get_SceneName() == _scene);

	if (m_pTempScene)
	{
		if (!sameSceneReload)
			m_pTempScene->PreLoadResources();
		else
			CDebug::Log(L"Load scene skip PreLoadResources (same scene): " + _scene);

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

	m_ePlayState = PlayState::Stopped;
	m_bSceneAwakened = false;
	m_strPlayStartSceneName = L"";
	m_bStepFrameRequested = false;

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
}
