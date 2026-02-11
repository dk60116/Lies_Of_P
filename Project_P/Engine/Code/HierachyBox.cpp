#include "epch.h"
#include "HierachyBox.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace
{
	wstring GenerateUniqueEmptyName(CScene* scene)
	{
		if (!scene)
			return L"GameObject";

		unordered_set<wstring> existingNames;
		for (auto* obj : scene->Get_ObjectList())
		{
			if (obj)
				existingNames.insert(obj->Get_ObjectName());
		}

		const wstring baseName = L"GameObject";
		if (existingNames.find(baseName) == existingNames.end())
			return baseName;

		for (_uint i = 1; i < 1000000; ++i)
		{
			const wstring candidate = baseName + L" (" + to_wstring(i) + L")";
			if (existingNames.find(candidate) == existingNames.end())
				return candidate;
		}

		return baseName + L" (New)";
	}

	CGameObject* CreateEmptyObject(CScene* scene, CGameObject* parent)
	{
		if (!scene)
			return nullptr;

		CGameObject* newObject = scene->Add_GameObject(GenerateUniqueEmptyName(scene));
		if (!newObject)
			return nullptr;

		if (parent)
		{
			newObject->Get_Transform()->SetParent(parent->Get_Transform());
			newObject->Get_Transform()->Set_LocalPosition(vector3::zero());
			newObject->Get_Transform()->Set_LocalQuaternion(quaternion::identity());
			newObject->Get_Transform()->Set_LocalScale(1.f);
		}

		CEditor::GetInstance().Set_SelectedGameObject(newObject);
		return newObject;
	}

	string ToLowerCopy(string value)
	{
		transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c) { return static_cast<char>(tolower(c)); });
		return value;
	}

	string TrimCopy(const string& value)
	{
		size_t start = 0;
		while (start < value.size() && isspace(static_cast<unsigned char>(value[start])))
			++start;

		if (start == value.size())
			return {};

		size_t end = value.size() - 1;
		while (end > start && isspace(static_cast<unsigned char>(value[end])))
			--end;

		return value.substr(start, end - start + 1);
	}
}

CHierachyBox::CHierachyBox()
{
}

CHierachyBox::~CHierachyBox()
{
	OnDestroy();
}

CHierachyBox* CHierachyBox::Create()
{
	CHierachyBox* newBox = new CHierachyBox();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"Hierachy";

	return newBox;
}

void CHierachyBox::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	_float width = static_cast<_float>(editorOption.hierachyWidth);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImVec2 panelSize = ImVec2(width, viewport->Size.y - editorOption.topBarHeight);

	ImGui::SetNextWindowPos
	(
		ImVec2(viewport->Pos.x + viewport->Size.x - editorOption.inspectorWidth, viewport->Pos.y + editorOption.topBarHeight),
		0,
		ImVec2(1.0f, 0.0f)
	);

	ImGui::SetNextWindowSize(panelSize);

	ImGui::Begin
	(
		CEngineString::WStringToString(m_strBoxName).c_str(),
		nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse
	);

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 6.f);

	if (currentScene)
	{
		ImGui::InputTextWithHint("##HierarchySearch", "Search...", m_searchBuffer.data(), m_searchBuffer.size());
		ImGui::Separator();

		const string filterText = TrimCopy(m_searchBuffer.data());
		const string filterLower = ToLowerCopy(filterText);
		CGameObject* selectedObject = editor.Get_SelectedGameObject();
		if (selectedObject != m_lastSelectedGameObject)
		{
			m_lastSelectedGameObject = selectedObject;
			m_scrollToSelected = selectedObject != nullptr;
			m_openToSelected = false;
		}

		if (ImGui::BeginChild("HierarchyScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			for (auto& obj : currentScene->Get_RootObjects())
				RenderObjectHierarchy(obj, filterLower);

			if (ImGui::BeginPopupContextWindow("HierarchyScrollContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty"))
				{
					if (CGameObject* created = CreateEmptyObject(currentScene, nullptr))
					{
						m_lastSelectedGameObject = created;
						m_scrollToSelected = true;
						m_openToSelected = true;
					}
				}
				ImGui::EndPopup();
			}
		}
		ImGui::EndChild();
	}

	ImGui::PopStyleVar();

	ImGui::End();
}

void CHierachyBox::OnDestroy()
{
}

bool CHierachyBox::ObjectMatchesFilter(CGameObject* _obj, const std::string& filterLower) const
{
	if (!_obj)
		return false;

	if (filterLower.empty())
		return true;

	const string name = ToLowerCopy(CEngineString::WStringToString(_obj->Get_ObjectName()));
	if (name.find(filterLower) != string::npos)
		return true;

	for (auto* child : _obj->Get_Transform()->Get_ChldList())
	{
		if (ObjectMatchesFilter(child->Get_GameObject(), filterLower))
			return true;
	}

	return false;
}

bool CHierachyBox::IsAncestorOfSelected(CGameObject* _obj, CGameObject* selected) const
{
	if (!_obj || !selected)
		return false;

	CTransform* parent = selected->Get_Transform()->Get_Parent();
	while (parent)
	{
		if (parent->Get_GameObject() == _obj)
			return true;
		parent = parent->Get_Parent();
	}

	return false;
}

void CHierachyBox::RenderObjectHierarchy(CGameObject* _obj, const string& _filterLower)
{
	if (!_obj)
		return;

	CEditor& editor = CEditor::GetInstance();
	const _bool filterActive = !_filterLower.empty();
	CGameObject* selectedObject = editor.Get_SelectedGameObject();

	string name = CEngineString::WStringToString(_obj->Get_ObjectName());
	const _bool matchesFilter = ObjectMatchesFilter(_obj, _filterLower);

	if (filterActive && !matchesFilter)
		return;

	_bool hasChildren = !_obj->Get_Transform()->Get_ChldList().empty();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;

	if (filterActive && hasChildren)
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);

	if (m_openToSelected && !filterActive && selectedObject && (selectedObject == _obj || IsAncestorOfSelected(_obj, selectedObject)))
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);

	if (_obj == editor.Get_SelectedGameObject())
		flags |= ImGuiTreeNodeFlags_Selected;

	if (!hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	_bool nodeOpen = ImGui::TreeNodeEx((name + "##" + to_string(reinterpret_cast<size_t>(_obj))).c_str(), flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		editor.Set_SelectedGameObject(_obj);

	if (ImGui::BeginPopupContextItem(("HierarchyItemContext##" + to_string(reinterpret_cast<size_t>(_obj))).c_str()))
	{
		if (ImGui::MenuItem("Create Empty"))
		{
			if (CGameObject* created = CreateEmptyObject(CSceneManager::GetInstance().Get_CrtScene(), _obj))
			{
				m_lastSelectedGameObject = created;
				m_scrollToSelected = true;
				m_openToSelected = true;
			}
		}

		if (ImGui::MenuItem("Delete"))
		{
			if (editor.Get_SelectedGameObject() == _obj)
				editor.Set_SelectedGameObject(nullptr);

			_obj->Destroy();
			ImGui::EndPopup();
			if (_obj == selectedObject)
				m_scrollToSelected = false;
			return;
		}
		ImGui::EndPopup();
	}

	if (_obj == selectedObject && m_scrollToSelected)
	{
		ImGui::SetScrollHereY(0.35f);
		m_scrollToSelected = false;
		m_openToSelected = false;
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		CEditor::GetInstance().MoveTo_SelectedGameObject(_obj);

	if (hasChildren && nodeOpen)
	{
		for (auto* child : _obj->Get_Transform()->Get_ChldList())
			RenderObjectHierarchy(child->Get_GameObject(), _filterLower);

		ImGui::TreePop();
	}
}
