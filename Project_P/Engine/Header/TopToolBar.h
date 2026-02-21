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
	void ShowEditMenu();
	void ShowProjectSettingsWindow();
	void ShowPlayButtons();
	void Show2DButton();
	void ShowFPS();

private:
	_bool m_bProjectSettingsWindowOpen;
};

NS_END
