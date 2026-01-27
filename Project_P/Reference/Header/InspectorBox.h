#pragma once

#include "EditorBox.h"

NS_BEGIN(Engine)

class ENGINE_DLL CInspectorBox final : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CInspectorBox();
	~CInspectorBox();

private:
	static CInspectorBox* Create();

public:
	void Render() override;
	void OnDestroy() override;

private:
	void ShowTransform(CGameObject* _obj);
	void ShowRectTransform(CGameObject* _obj);
	_float m_fRXDrag, m_fRYDrag, m_fRZDrag;
};

NS_END

