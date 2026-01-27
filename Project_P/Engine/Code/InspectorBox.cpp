#include "epch.h"
#include "InspectorBox.h"

CInspectorBox::CInspectorBox()
	: m_fRXDrag(0.f)
    , m_fRYDrag(0.f)
    , m_fRZDrag(0.f)
{
}

CInspectorBox::~CInspectorBox()
{
	OnDestroy();
}

CInspectorBox* CInspectorBox::Create()
{
	CInspectorBox* newBox = new CInspectorBox();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"Inspector";

	return newBox;
}

void CInspectorBox::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	_float width = static_cast<_float>(editorOption.inspectorWidth);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImVec2 panelSize = ImVec2(width, viewport->Size.y - editorOption.topBarHeight);

	ImGui::SetNextWindowPos
	(
		ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + editorOption.topBarHeight),
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

    CGameObject* selectedObj = editor.Get_SelectedGameObject();

    if (selectedObj)
    {
        _bool active = selectedObj->IsActive();

        _float baseY = ImGui::GetCursorPosY();

        Toggle_Begin();

        if (ImGui::Checkbox("##ActiveToggle", &active))
            selectedObj->SetActive(active);

        CGameObject* s_NameTarget = nullptr;
        string  s_EditName;
        if (s_NameTarget != selectedObj)
        {
            s_NameTarget = selectedObj;
            s_EditName = CEngineString::WStringToString(selectedObj->Get_ObjectName());
        }

        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetCursorPosY(baseY);
        ImGui::Text(("[" + to_string(selectedObj->Get_UniqueID()) + "] ").c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosY(baseY + 3.f);
        ImGui::SetNextItemWidth(140.0f);
        string label = "##ObjName" + to_string(selectedObj->Get_UniqueID());
        if (ImGui::InputText(label.c_str(), &s_EditName,
            ImGuiInputTextFlags_AutoSelectAll |
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            wstring targetName = CEngineString::StringToWString(s_EditName);
            selectedObj->Set_ObjectName(targetName);
        }

        Toggle_End();

        ImGui::Text(selectedObj->IsBoneTransform() ? "Bone" : "");

        if (!selectedObj->GetComponent<CRectTransform>())
            ShowTransform(selectedObj);
        else
            ShowRectTransform(selectedObj);
    }
    else
        ImGui::Text("No object selected.");

	ImGui::End();
}

void CInspectorBox::OnDestroy()
{
}

void CInspectorBox::ShowTransform(CGameObject* _obj)
{
    CTransform* transform = _obj->Get_Transform();

    if (transform)
    {
        ImGui::Text("Transform");

        const _float LabelWidth = 60.f;
        const _float boxWidth = 34.f;

        // Position
        _float3 position = transform->Get_LocalPosition();
        if (ImGui::BeginTable("Position Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            ImGui::TableSetColumnIndex(1);
            
            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &position.x, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &position.y, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &position.z, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        // Rotation
        _float3 rotation = transform->Get_LocalEulerAngles();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Rotation");
            ImGui::TableSetColumnIndex(1);

            const _float rotationDeg = 2.5f;

            // X
            _float prevX = m_fRXDrag;
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &rotation.x, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            _float prevY = m_fRYDrag;
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Z
            _float prevZ = m_fRZDrag;
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            prevX = 0.f;
            prevY = 0.f;
            prevZ = 0.f;

            ImGui::EndTable();
        }

        // Scale
        _float3 scale = transform->Get_LocalScale();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &scale.x, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &scale.y, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &scale.z, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }
    }
}

void CInspectorBox::ShowRectTransform(CGameObject* _obj)
{
    CRectTransform* rectTransform = dynamic_cast<CRectTransform*>(_obj->Get_Transform());

    if (rectTransform)
    {
        ImGui::Text("Rect Transform");

        const _float LabelWidth = 0.f;
        const _float boxWidth = 34.f;

        vector2 position = rectTransform->Get_AnchoredPosition();
        if (ImGui::BeginTable("Base Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Pos X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos X", &position.x, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Pos Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos Y", &position.y, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        _float width = rectTransform->Get_Width();
        _float height = rectTransform->Get_Height();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Width"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Width", &width, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Height"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Height", &height, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        vector2 pivot = rectTransform->Get_Pivot();
        if (ImGui::BeginTable("Pivot Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Pivot");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &pivot.x, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &pivot.y, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        CRectTransform::Anchors anchors = rectTransform->Get_Anchors();
        if (ImGui::TreeNode("Anchors"))
        {
            ImGui::Text("Min");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmin", &anchors.min.x, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymin", &anchors.min.y, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();

            ImGui::Text("Max");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmax", &anchors.max.x, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymax", &anchors.max.y, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::TreePop();
        }

        vector3 rotation = rectTransform->Get_LocalEulerAngles();
        ImGui::Text("Rotation");
        ImGui::SameLine();

        // X
        _float prevX = m_fRXDrag;
        ImGui::TextUnformatted("X");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##X", &rotation.x, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Y
        _float prevY = m_fRYDrag;
        ImGui::TextUnformatted("Y");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Z
        _float prevZ = m_fRZDrag;
        ImGui::TextUnformatted("Z");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        prevX = 0.f;
        prevY = 0.f;
        prevZ = 0.f;
    }
}
