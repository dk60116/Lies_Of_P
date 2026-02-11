#pragma once

#include "EditorBox.h"

#include <array>
#include <string>

NS_BEGIN(Engine)

class ENGINE_DLL CHierachyBox final : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CHierachyBox();
	~CHierachyBox();

public:
	void Render() override;
	void OnDestroy() override;

private:
	static CHierachyBox* Create();

private:
	void RenderObjectHierarchy(CGameObject* _obj, const string& _filterLower);
	bool ObjectMatchesFilter(CGameObject* _obj, const string& _filterLower) const;
	bool IsAncestorOfSelected(CGameObject* _obj, CGameObject* _selected) const;

private:
	std::array<char, 128> m_searchBuffer{};
	CGameObject* m_lastSelectedGameObject = nullptr;
	bool m_scrollToSelected = false;
	bool m_openToSelected = false;
};

NS_END
