#pragma once

#include "epch.h"
#include "Scene.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSceneManager final 
{
	SINGLETONCLASS(CSceneManager);

public:
	enum shadowQualityOptions { Low, Middle, High, SuperHigh, Ultra };

	struct LightSettings
	{
		shadowQualityOptions shadowQuality = SuperHigh;
		_uint shadowMapSize = 0;
	};

public:
	HRESULT Initialize();
	void Release();

public:
	CScene* CreateScene(CScene* _newScene, wstring _name);
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
	LightSettings m_sLightSetting;
};

NS_END

