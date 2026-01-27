#include "epch.h"
#include "SceneManager.h"

CSceneManager::CSceneManager()
	: m_pCrtScene(nullptr)
	, m_pTempScene(nullptr)
	, m_mSceneList({})
	, m_bLoading(false)
	, m_pEditorCamObj(nullptr)
	, m_pEditorCamera(nullptr)
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

	for (TRAVERSAL_ITER(m_mSceneList, it))
	{
		(*it).second->SceneRelease();
		Safe_Release((*it).second);
	}

	m_mSceneList.clear();
}

CScene* CSceneManager::CreateScene(CScene* _newScene, wstring _name)
{
	CScene* newScene = dynamic_cast<CScene*>(_newScene);
	newScene->Set_SceneName(_name);

	m_mSceneList.emplace(_name, newScene);

	newScene->AddRef();

	string filePath = "../Assets/Scenes/" + CEngineString::WStringToString(_name) + ".scene";

	if (!CResources::FileExists(filePath))
	{
		ofstream outFile(filePath);

		if (outFile.is_open())
		{
			outFile << "SceneName : " << CEngineString::WStringToString(_name);
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
	m_pCrtScene = nullptr;
	m_pCrtScene = m_pTempScene;
	m_pTempScene = nullptr;

	if (m_pCrtScene)
	{
		m_pCrtScene->Initialize();
		m_bLoading = false;
	}

	wstring file = m_pCrtScene->Get_SceneName() + L".scenedata";
	auto sceneTransformInfo = CResources::GetInstance().ReadSceneObjectTransformInfos(file);

	m_pCrtScene->Bind_ObjectsTransform(sceneTransformInfo);
	m_pCrtScene->Awake();
	m_pCrtScene->Start();
}

CCamera* CSceneManager::Get_EditorCamera()
{
	return m_pCrtScene->Get_EditorCamera();
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
