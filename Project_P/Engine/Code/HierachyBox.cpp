#include "epch.h"
#include "HierachyBox.h"

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

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.f);

	if (currentScene)
	{
		for (auto& obj : currentScene->Get_RootObjects())
			RenderObjectHierarchy(obj);
	}

	ImGui::PopStyleVar();

	ImGui::End();
}

void CHierachyBox::OnDestroy()
{
}

void CHierachyBox::RenderObjectHierarchy(CGameObject* _obj)
{
	if (!_obj)
		return;

	CEditor& editor = CEditor::GetInstance();

	string name = CEngineString::WStringToString(_obj->Get_ObjectName());

	_bool hasChildren = !_obj->Get_Transform()->Get_ChldList().empty();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;

	if (_obj == editor.Get_SelectedGameObject())
		flags |= ImGuiTreeNodeFlags_Selected;

	if (!hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	_bool nodeOpen = ImGui::TreeNodeEx((name + "##" + to_string(reinterpret_cast<size_t>(_obj))).c_str(), flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		editor.Set_SelectedGameObject(_obj);

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		CEditor::GetInstance().MoveTo_SelectedGameObject(_obj);

	if (hasChildren && nodeOpen)
	{
		for (auto* child : _obj->Get_Transform()->Get_ChldList())
			RenderObjectHierarchy(child->Get_GameObject());

		ImGui::TreePop();
	}
}
