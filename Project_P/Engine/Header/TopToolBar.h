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
	void Show2DButton();
	void ShowFPS();
};

NS_END

