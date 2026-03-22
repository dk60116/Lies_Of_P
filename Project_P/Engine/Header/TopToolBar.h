#pragma once
#include "EditorBox.h"
#include "NaviMesh.h"
#include <array>
#include <string>

NS_BEGIN(Engine)

class ENGINE_DLL CTopToolBar : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CTopToolBar();
	~CTopToolBar();

private:
	static CTopToolBar* Create();

public:
	void Render() override;
	void OnDestroy() override;

private:
	void ShowSelectSceneButton();
	void ShowSceneMenu();
	void ShowEditMenu();
	void ShowViewMenu();
	void ShowAIMenu();
	void ShowSceneSettingsWindow();
	void ShowProjectSettingsWindow();
	void ShowProjectSettingsTime();
	void ShowProjectSettingsPhysics();
	void ShowProjectSettingsLight();
	void ShowPlayButtons();
	void Show2DButton();
	void ShowFPS();
	void ShowGameStatusWindow();
	void ShowNavigationWindow();

private:
	_bool m_bProjectSettingsWindowOpen;
	_int m_iProjectSettingsSelection;
	_float m_fPendingFixedTimeStep;
	_float m_fPendingTimeScale;
	vector3 m_vPendingGravity;
	array<_uint, 32> m_arrPendingPhysicsLayerCollisionMasks;
	_int m_iPendingShadowQuality;
	_bool m_bSceneSettingsWindowOpen;
	_bool m_bNavigationWindowOpen;
	_bool m_bNavigationBuildSucceeded;
	_int m_iNavigationPolygonCount;
	string m_strNavigationResourceName;
	string m_strNavigationBuildStatus;
	EngineAI::CNaviMesh::NavBakeOptions m_sNavigationBakeOptions;
};

NS_END