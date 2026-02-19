#pragma once

#include "epch.h"
#include "Scene.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSceneManager final 
{
	SINGLETONCLASS(CSceneManager);

public:
	enum shadowQualityOptions { Low, Middle, High, SuperHigh, Ultra };

	struct TimeSettings
	{
		_float fixedTimeStep = 0.02f;
		_float timeSclae = 1.f;
	};

	struct PhysicsSettings
	{
	};

	struct LightSettings
	{
		shadowQualityOptions shadowQuality = SuperHigh;
		_uint shadowMapSize = 0;
	};

public:
	HRESULT Initialize();
	void Release();

public:
	CScene* CreateScene(CScene* _newScene, const wstring& _sceneName);
	CScene* Get_CrtScene();
	CScene* Get_TempScene();
	const map<wstring, CScene*>& Get_SceneList();
	const _bool Is_Loading() const;

	void LoadScene(wstring _scene);
	void LoadScene(CScene* _scene);
	void LoadComplete();

public:
	class CCamera* Get_EditorCamera();

public:
	const TimeSettings& Get_TimeSetting();
	const LightSettings& Get_LightSetting();
	void Set_ShadowQuality(const shadowQualityOptions option);

private:
	CScene* m_pCrtScene;
	CScene* m_pTempScene;
	map<wstring, CScene*> m_mSceneList;

	_bool m_bLoading;

	class CGameObject* m_pEditorCamObj;
	class CEditorCamera* m_pEditorCamera;

private:
	TimeSettings m_sTimeSetting;
	LightSettings m_sLightSetting;
};

NS_END

