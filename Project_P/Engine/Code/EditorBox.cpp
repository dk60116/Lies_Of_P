#include "epch.h"
#include "EditorBox.h"

CEditorBox::CEditorBox()
{
    m_strName = L"Editor Box";
}

HRESULT CEditorBox::Initialize()
{
    return S_OK;
}

void CEditorBox::Toggle_Begin()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(-1, -1));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

    _float baseY = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(baseY + 3.0f);
}

void CEditorBox::Toggle_End()
{
    ImGui::PopStyleVar(3);
}
