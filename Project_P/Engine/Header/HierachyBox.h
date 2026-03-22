#pragma once

#include "EditorBox.h"

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

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
	void RenderInsertionDropZone(CTransform* _targetParent, CGameObject* _beforeObject);
	bool TryInsertObject(CGameObject* _droppedObject, CTransform* _targetParent, CGameObject* _beforeObject);
	bool ObjectMatchesFilter(CGameObject* _obj, const string& _filterLower) const;
	bool IsAncestorOfSelected(CGameObject* _obj, CGameObject* _selected) const;

private:
	std::array<char, 128> m_searchBuffer{};
	CGameObject* m_lastSelectedGameObject = nullptr;
	bool m_scrollToSelected = false;
	bool m_openToSelected = false;

	std::unordered_set<CGameObject*> m_selectedObjects;
	CGameObject* m_lastClickedObject = nullptr;
	std::vector<CGameObject*> m_vFlatVisible;
	std::vector<CGameObject*> m_vFlatVisibleBuilding;
	std::vector<CGameObject*> m_pendingDeleteObjects;
	bool m_bRequestDelete = false;
};

NS_END

