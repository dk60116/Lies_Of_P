#pragma once
#include "EditorBox.h"
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
	void ShowAIMenu();
	void ShowViewMenu();
	void ShowNavigationWindow();
	void ShowSceneSettingsWindow();
	void ShowProjectSettingsWindow();
	void ShowProjectSettingsTime();
	void ShowProjectSettingsLight();
	void ShowPlayButtons();
	void Show2DButton();
	void ShowFPS();
	void ShowGameStatusWindow();
	void PerformNavigationBake();
	void CollectNavigationBakeStats(_int& outObjectCount, _int& outMeshCount, _int& outTriangleCount) const;
private:
	_bool m_bProjectSettingsWindowOpen;
	_int m_iProjectSettingsSelection;
	_float m_fPendingFixedTimeStep;
	_float m_fPendingTimeScale;
	_int m_iPendingShadowQuality;
	_bool m_bSceneSettingsWindowOpen;
	_bool m_bGameStatusWindowOpen;
	_bool m_bNavigationWindowOpen;
	_bool m_bNavigationStaticOnly;
	_float m_fNavAgentRadius;
	_float m_fNavAgentHeight;
	_float m_fNavMaxSlope;
	_float m_fNavStepHeight;
	_float m_fNavCellSize;
	_float m_fNavCellHeight;
	wstring m_strNavigationBakeStatus;
};
NS_END