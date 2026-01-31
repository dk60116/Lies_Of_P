#include "epch.h"
#include "AnimatorControllerEditorBox.h"

#include <fstream>
#include <sstream>
#include <cfloat>

static string ReadAllText(const fs::path& p)
{
    ifstream ifs(p, ios::binary);
    if (!ifs.is_open())
        return {};
    string s((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    return s;
}

static _bool WriteAllText(const fs::path& p, const string& text)
{
    ofstream ofs(p, ios::binary);
    if (!ofs.is_open())
    {
        CDebug::LogError("Failed Save AnimatorController - No path: " + p.generic_string());
        return false;
    }
    ofs.write(text.data(), (streamsize)text.size());

    CDebug::Log("Save AnimatorController: " + p.generic_string());

    return true;
}

static float DistancePointToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = p.x - a.x;
    const float wy = p.y - a.y;

    const float c1 = vx * wx + vy * wy;
    if (c1 <= 0.f)
    {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    const float c2 = vx * vx + vy * vy;
    if (c2 <= c1)
    {
        const float dx = p.x - b.x;
        const float dy = p.y - b.y;
        return sqrtf(dx * dx + dy * dy);
    }

    const float t = c1 / c2;
    const float px = a.x + t * vx;
    const float py = a.y + t * vy;
    const float dx = p.x - px;
    const float dy = p.y - py;
    return sqrtf(dx * dx + dy * dy);
}

CAnimatorControllerEditorBox::CAnimatorControllerEditorBox()
    : m_bOpen(false)
    , m_bLoaded(false)
    , m_eSelectType(ESelectType::None)
    , m_iSelectedParamIndex(-1)
    , m_eDeleteType(EDeleteType::None)
    , m_iDeleteParamIndex(-1)
    , m_strDeleteStateName("")
    , m_bRequestDeletePopup(false)
{
    m_strBoxName = L"AnimatorController";
}

CAnimatorControllerEditorBox::~CAnimatorControllerEditorBox()
{
    OnDestroy();
}

CAnimatorControllerEditorBox* CAnimatorControllerEditorBox::Create()
{
    auto* box = new CAnimatorControllerEditorBox();
    if (FAILED(box->Initialize()))
    {
        delete box;
        return nullptr;
    }
    return box;
}

void CAnimatorControllerEditorBox::OnDestroy()
{
    m_bOpen = false;
    m_bLoaded = false;
    m_path.clear();

    m_controllerName.clear();
    m_entryState.clear();

    m_params.clear();
    m_states.clear();
    m_transitions.clear();

    m_selectedState.clear();
    m_pan = ImVec2(0, 0);
    m_anyStatePos = ImVec2(10.f, 10.f);
    m_entryPos = ImVec2(10.f, 60.f);

    m_eSelectType = ESelectType::None;
    m_iSelectedParamIndex = -1;
    m_iSelectedTransitionIndex = -1;

    m_eDeleteType = EDeleteType::None;
    m_iDeleteParamIndex = -1;
    m_strDeleteStateName.clear();
    m_bRequestDeletePopup = false;

    m_motionOptions.clear();
    m_bMotionOptionsDirty = true;

    m_pendingTransitionFrom.clear();
    m_pendingSourceType = EPendingSource::None;
}

void CAnimatorControllerEditorBox::Open(const fs::path& path)
{
    if (!LoadFromFile(path))
        return;

    m_bOpen = true;
    m_bLoaded = true;
}

bool CAnimatorControllerEditorBox::LoadFromFile(const fs::path& path)
{
    error_code ec;
    if (!fs::exists(path, ec) || ec)
    {
        CDebug::LogError(L"AnimatorController open failed - not exists: " + path.wstring());
        return false;
    }

    string text = ReadAllText(path);
    if (text.empty())
    {
        CDebug::LogError(L"AnimatorController open failed - empty or cannot read: " + path.wstring());
        return false;
    }

    OnDestroy(); // 기존 데이터 초기화

    m_path = path;

    if (!ParseText(text))
    {
        CDebug::LogError(L"AnimatorController parse failed: " + path.wstring());
        return false;
    }

    // 기본 선택
    if (m_selectedState.empty() && !m_states.empty())
        m_selectedState = m_states.begin()->first;

    return true;
}

void CAnimatorControllerEditorBox::Render()
{
    if (!m_bLoaded)
        return;

    string title = "AnimatorController##" + m_path.string();
    if (!ImGui::Begin(title.c_str(), (_bool*)&m_bOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if (!m_bOpen)
    {
        ImGui::End();
        OnDestroy();
        return;
    }

    RenderToolbar();

    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    _float leftW = 220.f;
    _float rightW = 260.f;

    ImGui::BeginChild("LeftPanel", ImVec2(leftW, avail.y), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("GraphPanel", ImVec2(avail.x - leftW - rightW - 8.f, avail.y), true);
    RenderGraph();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("InspectorPanel", ImVec2(rightW, avail.y), true);
    RenderInspector();
    ImGui::EndChild();

    RenderDeleteConfirmPopup();
    RenderAddParamPopup();
    RenderAddStatePopup();

    ImGui::End();
}

void CAnimatorControllerEditorBox::RenderToolbar()
{
    ImGui::Text("File: %s", m_path.filename().string().c_str());

    ImGui::SameLine();
    if (ImGui::Button("Save"))
        SaveToFile();

    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        m_bOpen = false;
    }

    ImGui::SameLine();
    ImGui::Text("| name=%s  entry=%s", m_controllerName.c_str(), m_entryState.c_str());
}

void CAnimatorControllerEditorBox::RenderLeftPanel()
{
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (m_eSelectType == ESelectType::Param && m_iSelectedParamIndex >= 0)
            RequestDeleteParam(m_iSelectedParamIndex);
        else if (m_eSelectType == ESelectType::State && !m_selectedState.empty())
            RequestDeleteState(m_selectedState);
    }

    // ===== Parameters Header + Add 버튼 =====
    ImGui::Text("Parameters");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##AddParam"))
        RequestAddParam();
    ImGui::Separator();

    for (int i = 0; i < (int)m_params.size(); ++i)
    {
        const auto& p = m_params[i];

        string display;
        if (p.type == "trigger") display = p.type + " " + p.name;
        else display = p.type + " " + p.name + "=" + p.value;

        bool selected = (m_eSelectType == ESelectType::Param && m_iSelectedParamIndex == i);

        ImGui::PushID(i);
        if (ImGui::Selectable(display.c_str(), selected))
        {
            m_eSelectType = ESelectType::Param;
            m_iSelectedParamIndex = i;
        }

        if (ImGui::BeginPopupContextItem("ParamCtx"))
        {
            if (ImGui::MenuItem("Delete"))
                RequestDeleteParam(i);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();

    // ===== States Header + Add 버튼 =====
    ImGui::Text("States");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##AddState"))
        RequestAddState();
    ImGui::Separator();

    for (auto& kv : m_states)
    {
        const string& name = kv.first;
        _bool selected = (m_eSelectType == ESelectType::State && m_selectedState == name);

        ImGui::PushID(name.c_str());
        if (ImGui::Selectable(name.c_str(), selected))
        {
            m_eSelectType = ESelectType::State;
            m_selectedState = name;
        }

        if (ImGui::BeginPopupContextItem("StateCtx"))
        {
            if (ImGui::MenuItem("Delete"))
                RequestDeleteState(name);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void CAnimatorControllerEditorBox::RenderInspector()
{
    ImGui::Text("Inspector");
    ImGui::Separator();

    if (m_eSelectType == ESelectType::Transition &&
        m_iSelectedTransitionIndex >= 0 &&
        m_iSelectedTransitionIndex < (int)m_transitions.size())
    {
        Transition& tr = m_transitions[m_iSelectedTransitionIndex];
        const char* fromLabel = tr.isAny ? "AnyState" : tr.from.c_str();

        ImGui::Text("Transition");
        ImGui::Separator();
        ImGui::Text("From: %s", fromLabel);
        ImGui::Text("To: %s", tr.to.c_str());
        ImGui::DragFloat("Blend", &tr.blend, 0.01f, 0.0f, 5.0f);
        ImGui::Checkbox("Has Exit Time", &tr.hasExitTime);
        if (tr.hasExitTime)
            ImGui::DragFloat("Exit Time", &tr.exitTime, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Fixed Duration", &tr.fixedDuration);
        ImGui::DragFloat("Transition Duration", &tr.transitionDuration, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Transition Offset", &tr.transitionOffset, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Conditions");

        auto splitConditions = [&](const string& s)
            {
                vector<string> out;
                if (s.empty())
                    return out;
                string normalized = s;
                size_t pos = 0;
                while ((pos = normalized.find("&&")) != string::npos)
                    normalized.replace(pos, 2, ";");
                auto parts = Split(normalized, ';');
                for (auto& part : parts)
                {
                    string t = Trim(part);
                    if (!t.empty())
                        out.push_back(t);
                }
                return out;
            };

        static int lastTransitionIndex = -1;
        static int selectedCondIndex = -1;
        static int paramIndex = 0;
        static int opIndex = 0;
        static int boolValue = 0;
        static int intValue = 0;
        static float floatValue = 0.f;

        if (lastTransitionIndex != m_iSelectedTransitionIndex)
        {
            lastTransitionIndex = m_iSelectedTransitionIndex;
            selectedCondIndex = -1;
            paramIndex = 0;
            opIndex = 0;
            boolValue = 0;
            intValue = 0;
            floatValue = 0.f;
        }

        vector<string> conditions = splitConditions(tr.cond);
        if (!conditions.empty())
        {
            if (ImGui::BeginListBox("##ConditionList", ImVec2(-FLT_MIN, 80.f)))
            {
                for (int i = 0; i < (int)conditions.size(); ++i)
                {
                    const bool isSelected = (selectedCondIndex == i);
                    if (ImGui::Selectable(conditions[i].c_str(), isSelected))
                        selectedCondIndex = i;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
        }
        else
        {
            ImGui::TextDisabled("No conditions yet.");
        }

        if (!m_params.empty())
        {
            vector<const char*> paramNames;
            paramNames.reserve(m_params.size());
            for (const auto& p : m_params)
                paramNames.push_back(p.name.c_str());

            paramIndex = std::min(paramIndex, (int)paramNames.size() - 1);
            ImGui::Combo("Parameter", &paramIndex, paramNames.data(), (int)paramNames.size());

            const Param& param = m_params[paramIndex];
            static const char* opLabels[] = { "==", "!=", ">", ">=", "<", "<=" };

            if (param.type == "trigger")
            {
                ImGui::TextDisabled("Trigger condition uses parameter name only.");
            }
            else if (param.type == "bool")
            {
                ImGui::Combo("Operator", &opIndex, opLabels, 2);
                ImGui::Combo("Value", &boolValue, "false\0true\0");
            }
            else if (param.type == "int")
            {
                ImGui::Combo("Operator", &opIndex, opLabels, 6);
                ImGui::InputInt("Value", &intValue);
            }
            else if (param.type == "float")
            {
                ImGui::Combo("Operator", &opIndex, opLabels, 6);
                ImGui::InputFloat("Value", &floatValue);
            }

            if (ImGui::Button("Add Condition"))
            {
                string newCond;
                if (param.type == "trigger")
                {
                    newCond = param.name;
                }
                else if (param.type == "bool")
                {
                    newCond = param.name + string(opLabels[opIndex]) + (boolValue ? "true" : "false");
                }
                else if (param.type == "int")
                {
                    newCond = param.name + string(opLabels[opIndex]) + to_string(intValue);
                }
                else if (param.type == "float")
                {
                    newCond = param.name + string(opLabels[opIndex]) + to_string(floatValue);
                }

                if (!newCond.empty())
                {
                    if (!tr.cond.empty())
                        tr.cond += " && ";
                    tr.cond += newCond;
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No parameters available for conditions.");
        }

        if (selectedCondIndex >= 0 && selectedCondIndex < (int)conditions.size())
        {
            ImGui::SameLine();
            if (ImGui::Button("Remove Condition"))
            {
                conditions.erase(conditions.begin() + selectedCondIndex);
                selectedCondIndex = -1;

                tr.cond.clear();
                for (size_t i = 0; i < conditions.size(); ++i)
                {
                    if (i > 0)
                        tr.cond += " && ";
                    tr.cond += conditions[i];
                }
            }
        }

        ImGui::Separator();
        if (!tr.isAny)
        {
            if (ImGui::Button("Add Reverse Transition"))
            {
                AddTransition(tr.to, tr.from);
            }
        }
        else
        {
            ImGui::TextDisabled("Reverse transition is not supported for AnyState.");
        }

        if (ImGui::Button("Delete Transition"))
        {
            DeleteTransition(m_iSelectedTransitionIndex);
        }

        return;
    }

    // =========================
    // Parameter Inspector
    // =========================
    if (m_eSelectType == ESelectType::Param &&
        m_iSelectedParamIndex >= 0 &&
        m_iSelectedParamIndex < (int)m_params.size())
    {
        Param& p = m_params[m_iSelectedParamIndex];

        // (중요) rename 입력 버퍼를 선택된 인덱스에 바인딩 (멤버 버퍼)
        BindParamRenameBuffer(m_iSelectedParamIndex);

        ImGui::Text("Parameter");
        ImGui::Separator();

        ImGui::Text("Type: %s", p.type.c_str());

        // ---- Rename (Param Name) ----
        bool applyRename = false;
        applyRename |= ImGui::InputText("Name##ParamRename",
            m_editParamNameBuf.data(), m_editParamNameBuf.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        applyRename |= ImGui::Button("Apply##ParamRenameBtn");

        if (applyRename)
        {
            std::string err;
            if (!RenameParam(m_iSelectedParamIndex, m_editParamNameBuf.data(), &err))
            {
                m_strRenameError = err;
            }
            else
            {
                m_strRenameError.clear();
                // 정규화된 이름으로 다시 세팅
                strcpy_s(m_editParamNameBuf.data(), m_editParamNameBuf.size(),
                    m_params[m_iSelectedParamIndex].name.c_str());
            }
        }

        if (!m_strRenameError.empty())
        {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_strRenameError.c_str());
        }

        // ---- Value ---- (trigger는 value 없음)
        if (p.type != "trigger")
        {
            std::array<char, 128> valBuf{};
            strcpy_s(valBuf.data(), valBuf.size(), p.value.c_str());
            if (ImGui::InputText("Value##ParamValue", valBuf.data(), valBuf.size()))
                p.value = valBuf.data();
        }
        else
        {
            ImGui::TextDisabled("Trigger has no default value.");
        }

        return;
    }

    // =========================
    // State Inspector
    // =========================
    if (m_selectedState.empty())
    {
        ImGui::TextDisabled("No state selected.");
        return;
    }

    auto it = m_states.find(m_selectedState);
    if (it == m_states.end())
    {
        ImGui::TextDisabled("Invalid selection.");
        return;
    }

    // (중요) st 참조는 rename에서 map re-key가 발생하면 무효화될 수 있으니
    // rename 처리 후에는 다시 find 해서 st를 갱신한다.
    {
        const std::string currentName = it->second.name;
        BindStateRenameBuffer(currentName);

        ImGui::Text("State");
        ImGui::Separator();

        // ---- Rename (State Name) ----
        bool applyStateRename = false;
        applyStateRename |= ImGui::InputText("Name##StateRename",
            m_editStateNameBuf.data(), m_editStateNameBuf.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        applyStateRename |= ImGui::Button("Apply##StateRenameBtn");

        if (applyStateRename)
        {
            std::string err;
            if (!RenameState(currentName, m_editStateNameBuf.data(), &err))
            {
                m_strRenameError = err;
                // 실패 시 버퍼를 원복
                strcpy_s(m_editStateNameBuf.data(), m_editStateNameBuf.size(), currentName.c_str());
            }
            else
            {
                m_strRenameError.clear();
                // rename 성공 시 m_selectedState도 바뀌어 있을 수 있으니 버퍼/이터레이터 재바인딩
                BindStateRenameBuffer(m_selectedState);
            }

            // map이 바뀌었을 수 있으니 안전하게 다시 찾고 진행
            it = m_states.find(m_selectedState);
            if (it == m_states.end())
                return;
        }

        if (!m_strRenameError.empty())
        {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_strRenameError.c_str());
        }

        ImGui::Separator();
    }

    State& st = it->second;

    // ---- Motion Combo ----
    EnsureMotionOptionsLoaded();

    ImGui::Text("Motion");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh##MotionList"))
    {
        m_bMotionOptionsDirty = true;
        EnsureMotionOptionsLoaded();
    }

    const char* preview = st.motion.empty() ? "<None>" : st.motion.c_str();
    if (ImGui::BeginCombo("Motion##Combo", preview))
    {
        // None
        {
            bool sel = st.motion.empty();
            if (ImGui::Selectable("<None>", sel))
                st.motion.clear();
            if (sel) ImGui::SetItemDefaultFocus();
        }

        // Options
        for (const auto& opt : m_motionOptions)
        {
            bool sel = (st.motion == opt);
            if (ImGui::Selectable(opt.c_str(), sel))
                st.motion = opt;
            if (sel) ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    // ---- State Properties ----
    ImGui::DragFloat("SpeedMul", &st.speedMul, 0.01f, 0.0f, 10.0f);
    ImGui::Text("Pos: (%.1f, %.1f)", st.pos.x, st.pos.y);

    // ---- Transitions ----
    ImGui::Separator();
    ImGui::Text("Transitions (from this)");
    for (const auto& tr : m_transitions)
    {
        if (!tr.isAny && tr.from == st.name)
        {
            ImGui::BulletText("-> %s  blend=%.2f  %s",
                tr.to.c_str(),
                tr.blend,
                tr.cond.empty() ? "" : tr.cond.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Text("AnyState Transitions");
    for (const auto& tr : m_transitions)
    {
        if (tr.isAny)
        {
            ImGui::BulletText("Any -> %s  blend=%.2f  %s",
                tr.to.c_str(),
                tr.blend,
                tr.cond.empty() ? "" : tr.cond.c_str());
        }
    }
}

void CAnimatorControllerEditorBox::RenderGraph()
{
    ImGui::TextDisabled("Ctrl+Click to connect states (Entry/AnyState supported).");
    if (m_pendingSourceType != EPendingSource::None)
    {
        const char* label = "";
        if (m_pendingSourceType == EPendingSource::State)
            label = m_pendingTransitionFrom.c_str();
        else if (m_pendingSourceType == EPendingSource::AnyState)
            label = "AnyState";
        else if (m_pendingSourceType == EPendingSource::Entry)
            label = "Entry";
        ImGui::Text("Pending: %s -> ?", label);
    }

    ImGui::Separator();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        m_pan.x += d.x;
        m_pan.y += d.y;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        m_pendingTransitionFrom.clear();
        m_pendingSourceType = EPendingSource::None;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // ===== Grid =====
    {
        ImVec2 size = ImGui::GetContentRegionAvail();
        const float gridStep = 32.f;
        ImU32 col = IM_COL32(255, 255, 255, 20);

        float x0 = fmodf(m_pan.x, gridStep);
        float y0 = fmodf(m_pan.y, gridStep);

        for (float x = x0; x < size.x; x += gridStep)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), col);

        for (float y = y0; y < size.y; y += gridStep)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), col);
    }

    // ===== Node constants =====
    const ImVec2 nodeSize(160, 70);

    auto getNodeCenter = [&](const State& st) -> ImVec2
        {
            ImVec2 p = ImVec2(origin.x + st.pos.x + m_pan.x, origin.y + st.pos.y + m_pan.y);
            return ImVec2(p.x + nodeSize.x * 0.5f, p.y + nodeSize.y * 0.5f);
        };

    // ===== AnyState / Entry Blocks (pos/size) =====
    const ImVec2 anyPos = ImVec2(origin.x + m_anyStatePos.x + m_pan.x, origin.y + m_anyStatePos.y + m_pan.y);
    const ImVec2 anySize = ImVec2(120.f, 40.f);

    const ImVec2 entryPos = ImVec2(origin.x + m_entryPos.x + m_pan.x, origin.y + m_entryPos.y + m_pan.y);
    const ImVec2 entrySize = ImVec2(120.f, 40.f);

    auto rectCenter = [](ImVec2 p, ImVec2 s) { return ImVec2(p.x + s.x * 0.5f, p.y + s.y * 0.5f); };
    auto rectEdgePoint = [](ImVec2 p, ImVec2 s, ImVec2 target)
        {
            ImVec2 c = ImVec2(p.x + s.x * 0.5f, p.y + s.y * 0.5f);
            ImVec2 d = ImVec2(target.x - c.x, target.y - c.y);

            float dx = d.x;
            float dy = d.y;
            if (fabsf(dx) < 0.0001f && fabsf(dy) < 0.0001f)
                return c;

            float halfW = s.x * 0.5f;
            float halfH = s.y * 0.5f;
            float tX = (fabsf(dx) > 0.0001f) ? (halfW / fabsf(dx)) : FLT_MAX;
            float tY = (fabsf(dy) > 0.0001f) ? (halfH / fabsf(dy)) : FLT_MAX;
            float t = std::min(tX, tY);

            return ImVec2(c.x + dx * t, c.y + dy * t);
        };
    auto rectEdgePointOnSide = [](ImVec2 p, ImVec2 s, ImVec2 target, int side)
        {
            float left = p.x;
            float right = p.x + s.x;
            float top = p.y;
            float bottom = p.y + s.y;

            float x = max(left, std::min(target.x, right));
            float y = max(top, std::min(target.y, bottom));

            switch (side)
            {
            case 0: // left
                return ImVec2(left, y);
            case 1: // right
                return ImVec2(right, y);
            case 2: // top
                return ImVec2(x, top);
            case 3: // bottom
                return ImVec2(x, bottom);
            default:
                return ImVec2(x, y);
            }
        };
    auto drawArrowLine = [&](const ImVec2& a, const ImVec2& b, ImU32 col, float thickness)
        {
            dl->AddLine(a, b, col, thickness);
            ImVec2 dir = ImVec2(b.x - a.x, b.y - a.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.0001f)
            {
                dir.x /= len;
                dir.y /= len;
                ImVec2 perp = ImVec2(-dir.y, dir.x);
                const float arrowSize = 8.f;
                ImVec2 tip = b;
                ImVec2 left = ImVec2(b.x - dir.x * arrowSize + perp.x * (arrowSize * 0.5f),
                    b.y - dir.y * arrowSize + perp.y * (arrowSize * 0.5f));
                ImVec2 right = ImVec2(b.x - dir.x * arrowSize - perp.x * (arrowSize * 0.5f),
                    b.y - dir.y * arrowSize - perp.y * (arrowSize * 0.5f));
                dl->AddTriangleFilled(tip, left, right, col);
            }
        };

    const ImVec2 anyCenter = rectCenter(anyPos, anySize);
    const ImVec2 entryCenter = rectCenter(entryPos, entrySize);

    // ===== Transition lines first =====
    _int clickedTransition = -1;
    _int deleteTransition = -1;
    const ImU32 selectedCol = IM_COL32(255, 165, 0, 230);
    const ImU32 anyCol = IM_COL32(255, 200, 0, 200);
    const ImU32 stateCol = IM_COL32(120, 200, 255, 200);
    const float reverseOffset = 24.f;
    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    for (_int i = 0; i < (_int)m_transitions.size(); ++i)
    {
        const auto& tr = m_transitions[i];
        const bool isSelected = (m_eSelectType == ESelectType::Transition && m_iSelectedTransitionIndex == i);
        if (tr.isAny)
        {
            auto itTo = m_states.find(tr.to);
            if (itTo == m_states.end()) continue;

            ImVec2 toCenter = getNodeCenter(itTo->second);
            ImVec2 from = rectEdgePoint(anyPos, anySize, toCenter);
            ImVec2 to = rectEdgePoint(
                ImVec2(origin.x + itTo->second.pos.x + m_pan.x, origin.y + itTo->second.pos.y + m_pan.y),
                nodeSize,
                anyCenter);
            drawArrowLine(from, to, isSelected ? selectedCol : anyCol, 2.0f);

            if (DistancePointToSegment(mousePos, from, to) <= 6.f)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    clickedTransition = i;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    deleteTransition = i;
            }

            continue;
        }

        auto itFrom = m_states.find(tr.from);
        auto itTo = m_states.find(tr.to);
        if (itFrom == m_states.end() || itTo == m_states.end())
            continue;

        ImVec2 fromCenter = getNodeCenter(itFrom->second);
        ImVec2 toCenter = getNodeCenter(itTo->second);
        ImVec2 drawFrom = fromCenter;
        ImVec2 drawTo = toCenter;

        const bool hasReverse = TransitionExists(tr.to, tr.from, false);
        ImVec2 fromRectPos = ImVec2(origin.x + itFrom->second.pos.x + m_pan.x, origin.y + itFrom->second.pos.y + m_pan.y);
        ImVec2 toRectPos = ImVec2(origin.x + itTo->second.pos.x + m_pan.x, origin.y + itTo->second.pos.y + m_pan.y);
        if (hasReverse)
        {
            ImVec2 dir = ImVec2(toCenter.x - fromCenter.x, toCenter.y - fromCenter.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.0001f)
            {
                dir.x /= len;
                dir.y /= len;
                ImVec2 perp = ImVec2(-dir.y, dir.x);
                const string& minName = (tr.from < tr.to) ? tr.from : tr.to;
                const float sign = (tr.from == minName) ? 1.f : -1.f;
                ImVec2 offset = ImVec2(perp.x * reverseOffset * sign, perp.y * reverseOffset * sign);
                drawFrom = ImVec2(fromCenter.x + offset.x, fromCenter.y + offset.y);
                drawTo = ImVec2(toCenter.x + offset.x, toCenter.y + offset.y);
            }
        }
        else
        {
            drawFrom = fromCenter;
            drawTo = toCenter;
        }

        ImVec2 from = rectEdgePoint(fromRectPos, nodeSize, drawTo);
        ImVec2 to = rectEdgePoint(toRectPos, nodeSize, drawFrom);
        if (hasReverse)
        {
            ImVec2 dir = ImVec2(toCenter.x - fromCenter.x, toCenter.y - fromCenter.y);
            bool horizontal = fabsf(dir.x) >= fabsf(dir.y);
            const string& minName = (tr.from < tr.to) ? tr.from : tr.to;
            const float sign = (tr.from == minName) ? 1.f : -1.f;
            int side = 0;

            if (horizontal)
                side = (sign > 0.f) ? 2 : 3; // top/bottom
            else
                side = (sign > 0.f) ? 0 : 1; // left/right

            from = rectEdgePointOnSide(fromRectPos, nodeSize, drawTo, side);
            to = rectEdgePointOnSide(toRectPos, nodeSize, drawFrom, side);
        }

        drawArrowLine(from, to, isSelected ? selectedCol : stateCol, 2.0f);

        if (DistancePointToSegment(mousePos, from, to) <= 6.f)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                clickedTransition = i;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                deleteTransition = i;
        }
    }

    if (clickedTransition >= 0)
    {
        m_eSelectType = ESelectType::Transition;
        m_iSelectedTransitionIndex = clickedTransition;
    }
    else if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsAnyItemHovered())
    {
        m_eSelectType = ESelectType::None;
        m_iSelectedTransitionIndex = -1;
        m_iSelectedParamIndex = -1;
        m_selectedState.clear();
    }

    if (deleteTransition >= 0)
        DeleteTransition(deleteTransition);

    if (m_eSelectType == ESelectType::Transition &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        DeleteTransition(m_iSelectedTransitionIndex);
    }

    // ===== Draw AnyState block =====
    {
        dl->AddRectFilled(anyPos, ImVec2(anyPos.x + anySize.x, anyPos.y + anySize.y), IM_COL32(70, 70, 70, 220), 6.f);
        dl->AddText(ImVec2(anyPos.x + 10.f, anyPos.y + 12.f), IM_COL32(255, 255, 255, 255), "AnyState");
    }

    // ===== Draw Entry block =====
    {
        dl->AddRectFilled(entryPos, ImVec2(entryPos.x + entrySize.x, entryPos.y + entrySize.y), IM_COL32(70, 70, 70, 220), 6.f);
        dl->AddText(ImVec2(entryPos.x + 10.f, entryPos.y + 12.f), IM_COL32(255, 255, 255, 255), "Entry");
    }

    // ===== AnyState input =====
    {
        ImGui::SetCursorScreenPos(anyPos);
        ImGui::InvisibleButton("any_state_node", anySize);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl)
        {
            if (m_pendingSourceType == EPendingSource::AnyState)
            {
                m_pendingSourceType = EPendingSource::None;
            }
            else
            {
                m_pendingSourceType = EPendingSource::AnyState;
                m_pendingTransitionFrom.clear();
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            m_anyStatePos.x += d.x;
            m_anyStatePos.y += d.y;
        }
    }

    // ===== Entry input =====
    {
        ImGui::SetCursorScreenPos(entryPos);
        ImGui::InvisibleButton("entry_node", entrySize);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl)
        {
            if (m_pendingSourceType == EPendingSource::Entry)
            {
                m_pendingSourceType = EPendingSource::None;
            }
            else
            {
                m_pendingSourceType = EPendingSource::Entry;
                m_pendingTransitionFrom.clear();
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            m_entryPos.x += d.x;
            m_entryPos.y += d.y;
        }
    }

    // ===== Entry -> entryState line (single) =====
    {
        // entry 타겟 결정(없으면 첫 state)
        std::string entryTarget = m_entryState;
        if (entryTarget.empty() && !m_states.empty())
            entryTarget = m_states.begin()->first;

        auto itEntry = m_states.find(entryTarget);
        if (itEntry != m_states.end())
        {
            ImVec2 toCenter = getNodeCenter(itEntry->second);
            ImVec2 from = rectEdgePoint(entryPos, entrySize, toCenter);
            ImVec2 to = rectEdgePoint(
                ImVec2(origin.x + itEntry->second.pos.x + m_pan.x, origin.y + itEntry->second.pos.y + m_pan.y),
                nodeSize,
                entryCenter);

            drawArrowLine(from, to, IM_COL32(120, 255, 120, 220), 2.5f);
        }
        else
        {
            // entry가 깨졌을 때 표시(선택)
            dl->AddText(ImVec2(entryPos.x + 55.f, entryPos.y + 12.f), IM_COL32(255, 100, 100, 255), "!");
        }
    }

    // ===== Draw State nodes =====
    for (auto& kv : m_states)
    {
        State& st = kv.second;

        ImVec2 p = ImVec2(origin.x + st.pos.x + m_pan.x, origin.y + st.pos.y + m_pan.y);

        bool selected = (m_selectedState == st.name);

        ImU32 bg = selected ? IM_COL32(80, 140, 220, 220) : IM_COL32(60, 60, 60, 220);
        ImU32 bd = selected ? IM_COL32(180, 220, 255, 255) : IM_COL32(120, 120, 120, 255);

        dl->AddRectFilled(p, ImVec2(p.x + nodeSize.x, p.y + nodeSize.y), bg, 8.f);
        dl->AddRect(p, ImVec2(p.x + nodeSize.x, p.y + nodeSize.y), bd, 8.f, 0, 2.f);

        dl->AddText(ImVec2(p.x + 10, p.y + 10), IM_COL32(255, 255, 255, 255), st.name.c_str());
        dl->AddText(ImVec2(p.x + 10, p.y + 32), IM_COL32(200, 200, 200, 255), st.motion.c_str());

        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton(("node##" + st.name).c_str(), nodeSize);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            m_eSelectType = ESelectType::State;
            m_selectedState = st.name;
            m_iSelectedTransitionIndex = -1;

            if (ImGui::GetIO().KeyCtrl)
            {
                if (m_pendingSourceType == EPendingSource::Entry)
                {
                    m_entryState = st.name;
                    m_pendingSourceType = EPendingSource::None;
                    m_pendingTransitionFrom.clear();
                }
                else if (m_pendingSourceType == EPendingSource::AnyState)
                {
                    AddAnyTransition(st.name);
                    m_pendingSourceType = EPendingSource::None;
                    m_pendingTransitionFrom.clear();
                }
                else if (m_pendingSourceType == EPendingSource::State && m_pendingTransitionFrom == st.name)
                {
                    m_pendingSourceType = EPendingSource::None;
                    m_pendingTransitionFrom.clear();
                }
                else if (m_pendingSourceType == EPendingSource::State && !m_pendingTransitionFrom.empty())
                {
                    AddTransition(m_pendingTransitionFrom, st.name);
                    m_pendingSourceType = EPendingSource::None;
                    m_pendingTransitionFrom.clear();
                }
                else if (m_pendingTransitionFrom.empty())
                {
                    m_pendingTransitionFrom = st.name;
                    m_pendingSourceType = EPendingSource::State;
                }
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            st.pos.x += d.x;
            st.pos.y += d.y;
        }
    }
}

bool CAnimatorControllerEditorBox::SaveToFile()
{
    if (m_path.empty())
        return false;

    string out = SerializeText();
    return WriteAllText(m_path, out);
}

string CAnimatorControllerEditorBox::Trim(const string& s)
{
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b]))
        b++;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1])) 
        e--;
    return s.substr(b, e - b);
}

bool CAnimatorControllerEditorBox::StartsWith(const string& s, const char* prefix)
{
    size_t n = strlen(prefix);
    if (s.size() < n) return false;
    return equal(prefix, prefix + n, s.begin());
}

vector<string> CAnimatorControllerEditorBox::Split(const string& s, char delim)
{
    vector<string> out;
    stringstream ss(s);
    string tok;
    while (getline(ss, tok, delim))
        out.push_back(tok);
    return out;
}

_bool CAnimatorControllerEditorBox::TryParseVec2(const string& s, ImVec2& out)
{
    auto parts = Split(s, ',');
    if (parts.size() != 2) 
        return false;
    out.x = (float)atof(Trim(parts[0]).c_str());
    out.y = (float)atof(Trim(parts[1]).c_str());
    return true;
}

void CAnimatorControllerEditorBox::RequestDeleteParam(int idx)
{
    if (idx < 0 || idx >= (int)m_params.size()) 
        return;

    m_eDeleteType = EDeleteType::Param;
    m_iDeleteParamIndex = idx;
    m_strDeleteStateName.clear();
    m_bRequestDeletePopup = true;
}

void CAnimatorControllerEditorBox::RequestDeleteState(const string& name)
{
    if (name.empty())
        return;
    if (m_states.find(name) == m_states.end())
        return;

    m_eDeleteType = EDeleteType::State;
    m_strDeleteStateName = name;
    m_iDeleteParamIndex = -1;
    m_bRequestDeletePopup = true;
}

void CAnimatorControllerEditorBox::RenderDeleteConfirmPopup()
{
    if (m_bRequestDeletePopup)
    {
        ImGui::OpenPopup("ConfirmDeletePopup");
        m_bRequestDeletePopup = false;
    }

    if (ImGui::BeginPopupModal("ConfirmDeletePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (m_eDeleteType == EDeleteType::Param && m_iDeleteParamIndex >= 0)
        {
            const auto& p = m_params[m_iDeleteParamIndex];
            ImGui::Text("Delete Parameter?");
            ImGui::Separator();
            ImGui::Text("%s %s", p.type.c_str(), p.name.c_str());
        }
        else if (m_eDeleteType == EDeleteType::State && !m_strDeleteStateName.empty())
        {
            ImGui::Text("Delete State?");
            ImGui::Separator();
            ImGui::Text("%s", m_strDeleteStateName.c_str());
            ImGui::TextDisabled("Related transitions will be removed too.");
        }
        else
        {
            ImGui::Text("Nothing to delete.");
        }

        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            if (m_eDeleteType == EDeleteType::Param)
                DeleteParam(m_iDeleteParamIndex);
            else if (m_eDeleteType == EDeleteType::State)
                DeleteState(m_strDeleteStateName);

            m_eDeleteType = EDeleteType::None;
            m_iDeleteParamIndex = -1;
            m_strDeleteStateName.clear();

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0)))
        {
            m_eDeleteType = EDeleteType::None;
            m_iDeleteParamIndex = -1;
            m_strDeleteStateName.clear();

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void CAnimatorControllerEditorBox::DeleteParam(int idx)
{
    if (idx < 0 || idx >= (_int)m_params.size())
        return;

    m_params.erase(m_params.begin() + idx);

    // selection 보정
    if (m_eSelectType == ESelectType::Param)
    {
        if (m_params.empty())
        {
            m_iSelectedParamIndex = -1;
            m_eSelectType = ESelectType::None;
        }
        else
        {
            m_iSelectedParamIndex = std::min(idx, (int)m_params.size() - 1);
        }
    }
}

void CAnimatorControllerEditorBox::DeleteState(const string& name)
{
    auto it = m_states.find(name);
    if (it == m_states.end())
        return;

    // 1) 상태 삭제
    m_states.erase(it);

    // 2) 관련 전이 정리
    CleanupTransitionsForDeletedState(name);

    // 3) entry 보정
    if (m_entryState == name)
        m_entryState = m_states.empty() ? "" : m_states.begin()->first;

    // 4) 선택 보정
    if (m_selectedState == name)
        m_selectedState = m_states.empty() ? "" : m_states.begin()->first;

    if (m_states.empty())
        m_eSelectType = ESelectType::None;

    if (m_pendingTransitionFrom == name)
        m_pendingTransitionFrom.clear();

    if (m_pendingSourceType == EPendingSource::State && m_pendingTransitionFrom.empty())
        m_pendingSourceType = EPendingSource::None;

    if (m_eSelectType == ESelectType::Transition)
    {
        if (m_iSelectedTransitionIndex >= 0 && m_iSelectedTransitionIndex < (int)m_transitions.size())
        {
            const auto& tr = m_transitions[m_iSelectedTransitionIndex];
            if (tr.to == name || (!tr.isAny && tr.from == name))
            {
                m_eSelectType = ESelectType::None;
                m_iSelectedTransitionIndex = -1;
            }
        }
    }
}

void CAnimatorControllerEditorBox::CleanupTransitionsForDeletedState(const string& name)
{
    m_transitions.erase(
        remove_if(m_transitions.begin(), m_transitions.end(),
            [&](const Transition& tr)
            {
                if (tr.isAny) return tr.to == name;
                return tr.from == name || tr.to == name;
            }),
        m_transitions.end());
}

void CAnimatorControllerEditorBox::RequestAddParam()
{
    m_bRequestAddParamPopup = true;
    m_strCreateError.clear();

    m_iNewParamType = 0;
    m_iNewParamBool = 0;
    m_newParamName.fill(0);
    m_newParamValue.fill(0);

    strcpy_s(m_newParamValue.data(), m_newParamValue.size(), "0");
}

void CAnimatorControllerEditorBox::RequestAddState()
{
    m_bRequestAddStatePopup = true;
    m_strCreateError.clear();

    m_newStateName.fill(0);
    m_newStateMotion.fill(0);
    m_newStateSpeedMul = 1.f;

    strcpy_s(m_newStateName.data(), m_newStateName.size(), "NewState");

    m_bMotionOptionsDirty = true;
}

void CAnimatorControllerEditorBox::RenderAddCreatePopups()
{
    RenderAddParamPopup();
    RenderAddStatePopup();
}

void CAnimatorControllerEditorBox::RenderAddParamPopup()
{
    if (m_bRequestAddParamPopup)
    {
        ImGui::OpenPopup("AddParamPopup");
        m_bRequestAddParamPopup = false;
    }

    if (ImGui::BeginPopupModal("AddParamPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static const char* kTypes[] = { "float", "int", "bool", "trigger" };
        ImGui::Text("Add Parameter");
        ImGui::Separator();

        ImGui::Combo("Type", &m_iNewParamType, kTypes, IM_ARRAYSIZE(kTypes));
        ImGui::InputText("Name", m_newParamName.data(), m_newParamName.size());

        if (m_iNewParamType == 2) // bool
        {
            static const char* kBoolVals[] = { "false", "true" };
            ImGui::Combo("Value", &m_iNewParamBool, kBoolVals, IM_ARRAYSIZE(kBoolVals));
        }
        else if (m_iNewParamType != 3) // not trigger
        {
            ImGui::InputText("Value", m_newParamValue.data(), m_newParamValue.size());
        }
        else
        {
            ImGui::TextDisabled("Trigger has no default value.");
        }

        if (!m_strCreateError.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_strCreateError.c_str());
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            string type = kTypes[m_iNewParamType];
            string name = SanitizeIdentifier(Trim(m_newParamName.data()));

            if (name.empty())
            {
                m_strCreateError = "Name is empty.";
            }
            else if (ParamNameExists(name))
            {
                m_strCreateError = "Parameter name already exists.";
            }
            else
            {
                string value;
                if (type == "trigger")
                {
                    value.clear();
                }
                else if (type == "bool")
                {
                    value = (m_iNewParamBool == 1) ? "true" : "false";
                }
                else
                {
                    value = Trim(m_newParamValue.data());
                    if (value.empty())
                        value = "0";
                }

                AddParam(type, name, value);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_strCreateError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void CAnimatorControllerEditorBox::RenderAddStatePopup()
{
    if (m_bRequestAddStatePopup)
    {
        ImGui::OpenPopup("AddStatePopup");
        m_bRequestAddStatePopup = false;
    }

    if (ImGui::BeginPopupModal("AddStatePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Add State");
        ImGui::Separator();

        ImGui::InputText("Name", m_newStateName.data(), m_newStateName.size());

        // ---- Motion 선택 (콤보) ----
        EnsureMotionOptionsLoaded();

        ImGui::Text("Motion");
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh##NewStateMotionList"))
        {
            m_bMotionOptionsDirty = true;
            EnsureMotionOptionsLoaded();
        }

        const char* preview = (m_newStateMotion[0] == '\0') ? "<None>" : m_newStateMotion.data();
        if (ImGui::BeginCombo("##NewStateMotionCombo", preview))
        {
            // None
            {
                bool sel = (m_newStateMotion[0] == '\0');
                if (ImGui::Selectable("<None>", sel))
                    m_newStateMotion[0] = '\0';
                if (sel) ImGui::SetItemDefaultFocus();
            }

            // Options
            for (const auto& opt : m_motionOptions)
            {
                bool sel = (opt == std::string(m_newStateMotion.data()));
                if (ImGui::Selectable(opt.c_str(), sel))
                    strcpy_s(m_newStateMotion.data(), m_newStateMotion.size(), opt.c_str());
                if (sel) ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        // ----------------------------

        ImGui::DragFloat("SpeedMul", &m_newStateSpeedMul, 0.01f, 0.0f, 10.0f);

        if (!m_strCreateError.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_strCreateError.c_str());
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            string name = SanitizeIdentifier(Trim(m_newStateName.data()));
            if (name.empty())
            {
                m_strCreateError = "Name is empty.";
            }
            else
            {
                string uniq = MakeUniqueStateName(name);
                string motion = Trim(m_newStateMotion.data()); // 콤보에서 선택된 값
                AddState(uniq, motion, m_newStateSpeedMul);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_strCreateError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void CAnimatorControllerEditorBox::AddParam(const string& type, const string& name, const string& value)
{
    Param p{};
    p.type = type;
    p.name = name;
    p.value = value;
    m_params.push_back(p);

    m_eSelectType = ESelectType::Param;
    m_iSelectedParamIndex = (int)m_params.size() - 1;
}

void CAnimatorControllerEditorBox::AddState(const string& name, const string& motion, _float speedMul)
{
    State st{};
    st.name = name;
    st.motion = motion;
    st.speedMul = speedMul;

    // 겹치지 않게 배치(간단 그리드)
    const int col = 4;
    int x = (m_iStateSpawnIndex % col);
    int y = (m_iStateSpawnIndex / col);
    st.pos = ImVec2(100.f + x * 200.f, 100.f + y * 120.f);
    ++m_iStateSpawnIndex;

    m_states[name] = st;

    // entry가 비어있거나 유효하지 않으면 첫 state로 지정
    if (m_entryState.empty() || m_states.find(m_entryState) == m_states.end())
        m_entryState = name;

    m_eSelectType = ESelectType::State;
    m_selectedState = name;
}

_bool CAnimatorControllerEditorBox::ParamNameExists(const string& name) const
{
    for (const auto& p : m_params)
        if (p.name == name)
            return true;
    return false;
}

string CAnimatorControllerEditorBox::MakeUniqueStateName(const string& base) const
{
    if (m_states.find(base) == m_states.end())
        return base;

    for (int i = 1; i < 10000; ++i)
    {
        string n = base + "_" + to_string(i);
        if (m_states.find(n) == m_states.end())
            return n;
    }
    return base;
}

string CAnimatorControllerEditorBox::SanitizeIdentifier(const string& s)
{
    string out = s;
    if (out.empty())
        return out;

    for (char& c : out)
    {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '=' || c == '[' || c == ']')
        {
            c = '_';
        }
    }
    return out;
}

void CAnimatorControllerEditorBox::EnsureMotionOptionsLoaded()
{
    if (!m_bMotionOptionsDirty)
        return;

    RefreshMotionOptions();
}

void CAnimatorControllerEditorBox::RefreshMotionOptions()
{
    m_motionOptions.clear();
    m_bMotionOptionsDirty = false;

    // 예: fs::path sceneDir = CPath::GetInstance().Get_ProjectRoot() / "Assets" / "Scenes";
    fs::path sceneDir = fs::path("../Assets") / "Scenes";

    error_code ec;
    if (!fs::exists(sceneDir, ec) || ec)
    {
        CDebug::LogError("Scene folder not found: " + sceneDir.generic_string());
        return;
    }

    std::unordered_set<string> uniq;

    for (auto it = fs::directory_iterator(sceneDir, ec); it != fs::directory_iterator(); it.increment(ec))
    {
        if (ec) 
            break;

        const fs::path& p = it->path();
        if (!it->is_regular_file(ec))
            continue;
        if (p.extension() != ".scene") 
            continue;

        ifstream ifs(p, ios::binary);
        if (!ifs.is_open()) 
            continue;

        string line;
        while (getline(ifs, line))
        {
            string t = Trim(line);
            if (t.empty()) 
                continue;
            if (IsSceneCommentLine(t))
                continue;            
            if (!ContainsAnimationClipTag(t)) 
                continue;    

            string key = ExtractKeyBeforeColon(t);
            if (!key.empty())
                uniq.insert(key);
        }
    }

    m_motionOptions.assign(uniq.begin(), uniq.end());
    std::sort(m_motionOptions.begin(), m_motionOptions.end());

    CDebug::Log("Motion options loaded: " + std::to_string(m_motionOptions.size()));
}

string CAnimatorControllerEditorBox::ExtractKeyBeforeColon(const string& line)
{
    auto pos = line.find(':');

    if (pos == string::npos)
        return Trim(line);

    return Trim(line.substr(0, pos));
}

_bool CAnimatorControllerEditorBox::IsSceneCommentLine(const string& trimmedLine)
{
    return StartsWith(trimmedLine, "//");
}

_bool CAnimatorControllerEditorBox::ContainsAnimationClipTag(const string& trimmedLine)
{
    return trimmedLine.find("[Animation Clip]") != string::npos;
}

void CAnimatorControllerEditorBox::BindParamRenameBuffer(int idx)
{
    if (m_boundParamIndex == idx) 
        return;
    m_boundParamIndex = idx;
    m_editParamNameBuf.fill(0);

    if (idx >= 0 && idx < (int)m_params.size())
        strcpy_s(m_editParamNameBuf.data(), m_editParamNameBuf.size(), m_params[idx].name.c_str());

    m_strRenameError.clear();
}

void CAnimatorControllerEditorBox::BindStateRenameBuffer(const std::string& stateName)
{
    if (m_boundStateName == stateName) return;
    m_boundStateName = stateName;
    m_editStateNameBuf.fill(0);

    if (!stateName.empty())
        strcpy_s(m_editStateNameBuf.data(), m_editStateNameBuf.size(), stateName.c_str());

    m_strRenameError.clear();
}

bool CAnimatorControllerEditorBox::TransitionExists(const string& from, const string& to, _bool isAny) const
{
    for (const auto& tr : m_transitions)
    {
        if (tr.isAny != isAny)
            continue;
        if (tr.from == from && tr.to == to)
            return true;
    }
    return false;
}

void CAnimatorControllerEditorBox::AddTransition(const string& from, const string& to)
{
    if (from.empty() || to.empty() || from == to)
        return;
    if (m_states.find(from) == m_states.end() || m_states.find(to) == m_states.end())
        return;
    if (TransitionExists(from, to, false))
        return;

    Transition tr{};
    tr.from = from;
    tr.to = to;
    tr.blend = 0.15f;
    tr.hasExitTime = false;
    tr.exitTime = 1.f;
    tr.fixedDuration = false;
    tr.transitionDuration = 0.15f;
    tr.transitionOffset = 0.f;
    tr.cond.clear();
    tr.isAny = false;
    m_transitions.push_back(tr);
}

void CAnimatorControllerEditorBox::AddAnyTransition(const string& to)
{
    if (to.empty())
        return;
    if (m_states.find(to) == m_states.end())
        return;
    if (TransitionExists("", to, true))
        return;

    Transition tr{};
    tr.to = to;
    tr.blend = 0.15f;
    tr.hasExitTime = false;
    tr.exitTime = 1.f;
    tr.fixedDuration = false;
    tr.transitionDuration = 0.15f;
    tr.transitionOffset = 0.f;
    tr.cond.clear();
    tr.isAny = true;
    m_transitions.push_back(tr);
}

void CAnimatorControllerEditorBox::DeleteTransition(_int index)
{
    if (index < 0 || index >= (_int)m_transitions.size())
        return;

    m_transitions.erase(m_transitions.begin() + index);

    if (m_eSelectType == ESelectType::Transition)
    {
        if (m_transitions.empty())
        {
            m_iSelectedTransitionIndex = -1;
            m_eSelectType = ESelectType::None;
        }
        else
        {
            m_iSelectedTransitionIndex = std::min(index, (_int)m_transitions.size() - 1);
        }
    }
}

bool CAnimatorControllerEditorBox::ParamNameExistsExcept(const string& name, int exceptIdx) const
{
    for (int i = 0; i < (int)m_params.size(); ++i)
    {
        if (i == exceptIdx) 
            continue;
        if (m_params[i].name == name) 
            return true;
    }
    return false;
}

bool CAnimatorControllerEditorBox::RenameParam(int idx, const string& newNameIn, string* outError)
{
    if (idx < 0 || idx >= (int)m_params.size())
    {
        if (outError) *outError = "Invalid parameter index.";
        return false;
    }

    string sanitized = SanitizeIdentifier(Trim(newNameIn));
    if (sanitized.empty())
    {
        if (outError) *outError = "Name is empty.";
        return false;
    }

    if (ParamNameExistsExcept(sanitized, idx))
    {
        if (outError) *outError = "Parameter name already exists.";
        return false;
    }

    m_params[idx].name = sanitized;
    return true;
}

bool CAnimatorControllerEditorBox::RenameState(const string& oldNameIn, const string& newNameIn, string* outError)
{
    string oldName = Trim(oldNameIn);
    string newName = SanitizeIdentifier(Trim(newNameIn));

    if (oldName.empty())
    {
        if (outError) *outError = "Old state name is empty.";
        return false;
    }
    if (newName.empty())
    {
        if (outError) *outError = "Name is empty.";
        return false;
    }
    if (oldName == newName)
        return true;

    auto it = m_states.find(oldName);
    if (it == m_states.end())
    {
        if (outError) *outError = "State not found.";
        return false;
    }
    if (m_states.find(newName) != m_states.end())
    {
        if (outError) *outError = "State name already exists.";
        return false;
    }

    // 1) key 변경
    State st = it->second;
    m_states.erase(it);
    st.name = newName;
    m_states.emplace(newName, st);

    // 2) transitions 동기화
    for (auto& tr : m_transitions)
    {
        if (tr.isAny)
        {
            if (tr.to == oldName) tr.to = newName;
        }
        else
        {
            if (tr.from == oldName) tr.from = newName;
            if (tr.to == oldName)   tr.to = newName;
        }
    }

    // 3) entry/selection 동기화
    if (m_entryState == oldName)    m_entryState = newName;
    if (m_selectedState == oldName) m_selectedState = newName;

    // 4) delete 대상도 동기화(안전)
    if (m_strDeleteStateName == oldName) m_strDeleteStateName = newName;

    return true;
}

_bool CAnimatorControllerEditorBox::ParseText(const string& text)
{
    m_controllerName.clear();
    m_entryState.clear();
    m_params.clear();
    m_states.clear();
    m_transitions.clear();

    enum class Sec { None, Params, State, Transition, Any };
    Sec sec = Sec::None;

    string curState;
    string firstStateName;
    Transition curTr{};
    _bool buildingTransition = false;

    istringstream iss(text);
    string line;

    auto flushTransition = [&]()
        {
            if (buildingTransition)
            {
                m_transitions.push_back(curTr);
                buildingTransition = false;
                curTr = Transition{};
            }
        };

    while (getline(iss, line))
    {
        line = Trim(line);
        if (line.empty()) 
            continue;
        if (StartsWith(line, "#"))
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            flushTransition();

            string secName = line.substr(1, line.size() - 2);
            secName = Trim(secName);

            if (secName == "parameters")
            {
                sec = Sec::Params;
                continue;
            }

            if (secName == "any")
            {
                sec = Sec::Any;

                buildingTransition = true;
                curTr = Transition{};
                curTr.isAny = true;
                curTr.fixedDuration = false;
                curTr.transitionDuration = 0.15f;
                curTr.transitionOffset = 0.f;
                continue;
            }

            if (StartsWith(secName, "state "))
            {
                sec = Sec::State;
                curState = Trim(secName.substr(6));
                if (!curState.empty())
                {
                    State st{};
                    st.name = curState;
                    st.motion = "";
                    st.speedMul = 1.f;
                    st.pos = ImVec2(100, 100);
                    m_states[curState] = st;
                    m_selectedState = curState;
                    if (firstStateName.empty())
                        firstStateName = curState;
                }
                continue;
            }

            // [transition A->B]
            if (StartsWith(secName, "transition "))
            {
                sec = Sec::Transition;
                string trName = Trim(secName.substr(11));

                auto arrow = trName.find("->");
                if (arrow == string::npos)
                    continue;

                string from = Trim(trName.substr(0, arrow));
                string to = Trim(trName.substr(arrow + 2));

                buildingTransition = true;
                curTr = Transition{};
                curTr.from = from;
                curTr.to = to;
                curTr.isAny = false;
                curTr.blend = 0.15f;
                curTr.hasExitTime = false;
                curTr.exitTime = 1.f;
                curTr.fixedDuration = false;
                curTr.transitionDuration = 0.15f;
                curTr.transitionOffset = 0.f;
                curTr.cond.clear();
                continue;
            }

            sec = Sec::None;
            continue;
        }

        // key=value
        auto eq = line.find('=');

        if (sec == Sec::None)
        {
            if (eq != string::npos)
            {
                string k = Trim(line.substr(0, eq));
                string v = Trim(line.substr(eq + 1));
                if (k == "name")
                    m_controllerName = v;
                else if (k == "entry")
                    m_entryState = v;
            }
            continue;
        }

        if (sec == Sec::Params)
        {
            Param p = {};
            auto parts = Split(line, ' ');
            if (parts.size() >= 2)
            {
                p.type = Trim(parts[0]);
                string rest = Trim(line.substr(p.type.size() + 1)); // "speed=0" or "attack"

                auto eq2 = rest.find('=');
                if (eq2 == string::npos)
                {
                    p.name = Trim(rest);
                    p.value.clear();
                }
                else
                {
                    p.name = Trim(rest.substr(0, eq2));
                    p.value = Trim(rest.substr(eq2 + 1));
                }

                m_params.push_back(p);
            }
            continue;
        }

        if (sec == Sec::State)
        {
            if (curState.empty()) continue;

            auto it = m_states.find(curState);
            if (it == m_states.end()) continue;

            State& st = it->second;

            if (eq != string::npos)
            {
                string k = Trim(line.substr(0, eq));
                string v = Trim(line.substr(eq + 1));

                if (k == "motion") st.motion = v;
                else if (k == "speedMul") st.speedMul = (float)atof(v.c_str());
                else if (k == "pos") TryParseVec2(v, st.pos);
            }
            continue;
        }

        // Transition / Any
        if ((sec == Sec::Transition || sec == Sec::Any) && buildingTransition)
        {
            if (eq == string::npos)
                continue;

            string k = Trim(line.substr(0, eq));
            string v = Trim(line.substr(eq + 1));

            if (k == "to")
            {
                curTr.to = v;
            }
            else if (k == "blend")
            {
                curTr.blend = (float)atof(v.c_str());
            }
            else if (k == "exitTime")
            {
                curTr.hasExitTime = true;
                curTr.exitTime = (float)atof(v.c_str());
            }
            else if (k == "fixedDuration")
            {
                curTr.fixedDuration = (v == "true" || v == "1");
            }
            else if (k == "transitionDuration" || k == "translationDuration")
            {
                curTr.transitionDuration = (float)atof(v.c_str());
            }
            else if (k == "transitionOffset" || k == "traslationOffset" || k == "translationOffset")
            {
                curTr.transitionOffset = (float)atof(v.c_str());
            }
            else if (k == "cond")
            {
                curTr.cond = v;
            }

            continue;
        }
    }

    flushTransition();

    // 기본값: controllerName 없으면 파일명에서
    if (m_controllerName.empty() && !m_path.empty())
        m_controllerName = m_path.stem().string();

    // entry가 없으면 첫 state
    if (m_entryState.empty())
    {
        if (!firstStateName.empty())
            m_entryState = firstStateName;
        else if (!m_states.empty())
            m_entryState = m_states.begin()->first;
    }

    return true;
}

string CAnimatorControllerEditorBox::SerializeText() const
{
    string t;
    t += "# AnimatorController v1\n";
    t += "name=" + m_controllerName + "\n";
    t += "entry=" + (m_entryState.empty() ? "Idle" : m_entryState) + "\n\n";

    t += "[parameters]\n";
    for (const auto& p : m_params)
    {
        if (p.type == "trigger")
            t += p.type + " " + p.name + "\n";
        else
            t += p.type + " " + p.name + "=" + p.value + "\n";
    }
    t += "\n";

    // states
    // (출력 순서 고정 원하면 vector로 정렬하세요)
    for (const auto& kv : m_states)
    {
        const auto& st = kv.second;
        t += "[state " + st.name + "]\n";
        t += "motion=" + st.motion + "\n";
        t += "speedMul=" + to_string(st.speedMul) + "\n";
        t += "pos=" + to_string((int)st.pos.x) + "," + to_string((int)st.pos.y) + "\n\n";
    }

    // transitions (any 먼저)
    for (const auto& tr : m_transitions)
    {
        if (!tr.isAny) continue;
        t += "[any]\n";
        t += "to=" + tr.to + "\n";
        t += "blend=" + to_string(tr.blend) + "\n";
        t += "fixedDuration=" + string(tr.fixedDuration ? "true" : "false") + "\n";
        t += "transitionDuration=" + to_string(tr.transitionDuration) + "\n";
        t += "transitionOffset=" + to_string(tr.transitionOffset) + "\n";
        if (!tr.cond.empty()) t += "cond=" + tr.cond + "\n";
        t += "\n";
    }

    for (const auto& tr : m_transitions)
    {
        if (tr.isAny) 
            continue;
        t += "[transition " + tr.from + "->" + tr.to + "]\n";
        t += "blend=" + to_string(tr.blend) + "\n";
        if (!tr.cond.empty())
            t += "cond=" + tr.cond + "\n";
        if (tr.hasExitTime) 
            t += "exitTime=" + to_string(tr.exitTime) + "\n";
        t += "fixedDuration=" + string(tr.fixedDuration ? "true" : "false") + "\n";
        t += "transitionDuration=" + to_string(tr.transitionDuration) + "\n";
        t += "transitionOffset=" + to_string(tr.transitionOffset) + "\n";
        t += "\n";
    }

    return t;
}
