#include "epch.h"
#include "Resources.h"
#include "AnimatorController.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace
{
    string Trim(const string& s)
    {
        size_t b = 0;
        while (b < s.size() && isspace(static_cast<unsigned char>(s[b])))
            ++b;
        size_t e = s.size();
        while (e > b && isspace(static_cast<unsigned char>(s[e - 1])))
            --e;
        return s.substr(b, e - b);
    }

    bool StartsWith(const string& s, const char* prefix)
    {
        size_t n = strlen(prefix);
        if (s.size() < n)
            return false;
        return equal(prefix, prefix + n, s.begin());
    }

    string ReadAllText(const fs::path& p)
    {
        ifstream ifs(p, ios::binary);
        if (!ifs.is_open())
            return {};
        return string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    }

    vector<string> Split(const string& s, char delim)
    {
        vector<string> out;
        stringstream ss(s);
        string tok;
        while (getline(ss, tok, delim))
            out.push_back(tok);
        return out;
    }

    vector<string> SplitConditions(const string& s)
    {
        string normalized = s;
        size_t pos = 0;
        while ((pos = normalized.find("&&", pos)) != string::npos)
        {
            normalized.replace(pos, 2, ",");
            pos += 1;
        }
        auto parts = Split(normalized, ',');
        vector<string> out;
        for (auto& part : parts)
        {
            string t = Trim(part);
            if (!t.empty())
                out.push_back(t);
        }
        return out;
    }

    bool ParseBool(const string& s, bool defaultValue = false)
    {
        string t = Trim(s);
        if (t == "true" || t == "1")
            return true;
        if (t == "false" || t == "0")
            return false;
        return defaultValue;
    }

    bool ParseCondition(
        const string& text,
        const unordered_map<string, CAnimatorController::PARAM_TYPE>& paramTypes,
        CAnimatorController::Condition& out)
    {
        string t = Trim(text);
        if (t.empty())
            return false;

        struct OpToken
        {
            const char* token;
            CAnimatorController::COMPARE_OP op;
        };

        static const OpToken kOps[] = {
            {"==", CAnimatorController::COMPARE_OP::EQUAL},
            {"!=", CAnimatorController::COMPARE_OP::NOT_EQUAL},
            {">=", CAnimatorController::COMPARE_OP::GREATER_EQUAL},
            {"<=", CAnimatorController::COMPARE_OP::LESS_EQUAL},
            {">", CAnimatorController::COMPARE_OP::GREATER},
            {"<", CAnimatorController::COMPARE_OP::LESS},
        };

        string lhs;
        string rhs;
        bool foundOp = false;
        CAnimatorController::COMPARE_OP op = CAnimatorController::COMPARE_OP::EQUAL;

        for (const auto& candidate : kOps)
        {
            size_t p = t.find(candidate.token);
            if (p != string::npos)
            {
                lhs = Trim(t.substr(0, p));
                rhs = Trim(t.substr(p + strlen(candidate.token)));
                op = candidate.op;
                foundOp = true;
                break;
            }
        }

        if (!foundOp)
        {
            lhs = t;
            rhs.clear();
            op = CAnimatorController::COMPARE_OP::EQUAL;
        }

        out.paramName = CEngineString::StringToWString(lhs);
        out.op = op;

        auto it = paramTypes.find(lhs);
        if (it == paramTypes.end())
        {
            if (!rhs.empty())
            {
                if (rhs == "true" || rhs == "false")
                    out.b = ParseBool(rhs);
                else if (rhs.find('.') != string::npos)
                    out.f = static_cast<_float>(atof(rhs.c_str()));
                else
                    out.i = atoi(rhs.c_str());
            }
            else
            {
                out.b = true;
                out.i = 1;
                out.f = 1.f;
            }
            return true;
        }

        switch (it->second)
        {
        case CAnimatorController::PARAM_TYPE::BOOL:
            out.b = ParseBool(rhs, true);
            break;
        case CAnimatorController::PARAM_TYPE::INT:
            out.i = rhs.empty() ? 0 : atoi(rhs.c_str());
            break;
        case CAnimatorController::PARAM_TYPE::FLOAT:
            out.f = rhs.empty() ? 0.f : static_cast<_float>(atof(rhs.c_str()));
            break;
        case CAnimatorController::PARAM_TYPE::TRIGGER:
            out.b = true;
            out.i = 1;
            out.f = 1.f;
            break;
        }

        return true;
    }

    void WriteWString(ofstream& out, const wstring& s)
    {
        _uint len = static_cast<_uint>(s.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(_uint));
        if (len)
            out.write(reinterpret_cast<const char*>(s.data()), sizeof(wchar_t) * len);
    }
}

HRESULT CResources::ConvertAnimatorControllerToBinary(const wstring _filePath)
{
    fs::path fullPath = m_strDefaultAssetPath + _filePath;

    string text = ReadAllText(fullPath);
    if (text.empty())
    {
        CDebug::LogError(L"ConvertAnimatorControllerToBinary failed - empty or missing file: " + fullPath.wstring());
        return E_FAIL;
    }

    enum class Section
    {
        None,
        Params,
        State,
        Transition,
        Any
    };

    struct TempTransition
    {
        wstring from;
        wstring to;
        _float blend = 0.15f;
        _bool hasExit = false;
        _float exitTime = 1.f;
        vector<string> conds = {};
        _bool isAny = false;
    };

    CAnimatorController::AnimatorControllerInitInfo info{};
    unordered_map<string, CAnimatorController::PARAM_TYPE> paramTypeLookup;
    unordered_map<wstring, CAnimatorController::State> states;
    vector<wstring> stateOrder;
    vector<TempTransition> tempTransitions;

    Section sec = Section::None;
    wstring currentState;
    TempTransition currentTransition{};
    _bool buildingTransition = false;

    auto flushTransition = [&]()
        {
            if (buildingTransition)
            {
                tempTransitions.push_back(currentTransition);
                currentTransition = TempTransition{};
                buildingTransition = false;
            }
        };

    istringstream iss(text);
    string line;
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

            string secName = Trim(line.substr(1, line.size() - 2));
            if (secName == "parameters")
            {
                sec = Section::Params;
                continue;
            }
            if (secName == "any")
            {
                sec = Section::Any;
                buildingTransition = true;
                currentTransition = TempTransition{};
                currentTransition.isAny = true;
                continue;
            }
            if (StartsWith(secName, "state "))
            {
                sec = Section::State;
                currentState = CEngineString::StringToWString(Trim(secName.substr(6)));
                if (!currentState.empty() && states.find(currentState) == states.end())
                {
                    CAnimatorController::State st{};
                    st.name = currentState;
                    st.motionName = L"";
                    st.speedMul = 1.f;
                    states.emplace(currentState, st);
                    stateOrder.push_back(currentState);
                }
                continue;
            }
            if (StartsWith(secName, "transition "))
            {
                sec = Section::Transition;
                string trName = Trim(secName.substr(11));
                auto arrow = trName.find("->");
                if (arrow == string::npos)
                    continue;
                string from = Trim(trName.substr(0, arrow));
                string to = Trim(trName.substr(arrow + 2));

                buildingTransition = true;
                currentTransition = TempTransition{};
                currentTransition.isAny = false;
                currentTransition.from = CEngineString::StringToWString(from);
                currentTransition.to = CEngineString::StringToWString(to);
                continue;
            }

            sec = Section::None;
            continue;
        }

        auto eq = line.find('=');

        if (sec == Section::None)
        {
            if (eq == string::npos)
                continue;
            string k = Trim(line.substr(0, eq));
            string v = Trim(line.substr(eq + 1));
            if (k == "name")
                info.controllerName = CEngineString::StringToWString(v);
            else if (k == "entry")
                info.entryState = CEngineString::StringToWString(v);
            continue;
        }

        if (sec == Section::Params)
        {
            auto parts = Split(line, ' ');
            if (parts.size() < 2)
                continue;

            string typeStr = Trim(parts[0]);
            string rest = Trim(line.substr(typeStr.size() + 1));
            string name;
            string value;
            auto eq2 = rest.find('=');
            if (eq2 == string::npos)
            {
                name = Trim(rest);
                value.clear();
            }
            else
            {
                name = Trim(rest.substr(0, eq2));
                value = Trim(rest.substr(eq2 + 1));
            }

            CAnimatorController::ParameterDesc desc{};
            desc.name = CEngineString::StringToWString(name);

            if (typeStr == "bool")
            {
                desc.type = CAnimatorController::PARAM_TYPE::BOOL;
                desc.defaultBool = ParseBool(value);
            }
            else if (typeStr == "int")
            {
                desc.type = CAnimatorController::PARAM_TYPE::INT;
                desc.defaultInt = value.empty() ? 0 : atoi(value.c_str());
            }
            else if (typeStr == "float")
            {
                desc.type = CAnimatorController::PARAM_TYPE::FLOAT;
                desc.defaultFloat = value.empty() ? 0.f : static_cast<_float>(atof(value.c_str()));
            }
            else
            {
                desc.type = CAnimatorController::PARAM_TYPE::TRIGGER;
            }

            info.parameters.push_back(desc);
            paramTypeLookup[name] = desc.type;
            continue;
        }

        if (sec == Section::State)
        {
            if (currentState.empty())
                continue;
            auto it = states.find(currentState);
            if (it == states.end())
                continue;
            if (eq == string::npos)
                continue;

            string k = Trim(line.substr(0, eq));
            string v = Trim(line.substr(eq + 1));

            if (k == "motion")
                it->second.motionName = CEngineString::StringToWString(v);
            else if (k == "speedMul")
                it->second.speedMul = static_cast<_float>(atof(v.c_str()));

            continue;
        }

        if ((sec == Section::Transition || sec == Section::Any) && buildingTransition)
        {
            if (eq == string::npos)
                continue;

            string k = Trim(line.substr(0, eq));
            string v = Trim(line.substr(eq + 1));

            if (k == "to")
                currentTransition.to = CEngineString::StringToWString(v);
            else if (k == "blend")
                currentTransition.blend = static_cast<_float>(atof(v.c_str()));
            else if (k == "exitTime")
            {
                currentTransition.hasExit = true;
                currentTransition.exitTime = static_cast<_float>(atof(v.c_str()));
            }
            else if (k == "cond")
            {
                auto condParts = SplitConditions(v);
                currentTransition.conds.insert(currentTransition.conds.end(), condParts.begin(), condParts.end());
            }
            continue;
        }
    }

    flushTransition();

    if (info.controllerName.empty())
        info.controllerName = fullPath.stem().wstring();

    if (info.entryState.empty() && !stateOrder.empty())
        info.entryState = stateOrder.front();

    for (const auto& name : stateOrder)
        info.states.push_back(states[name]);

    for (const auto& temp : tempTransitions)
    {
        CAnimatorController::Transition tr{};
        tr.toState = temp.to;
        tr.blendDuration = temp.blend;
        tr.hasExitTime = temp.hasExit;
        tr.exitTimeNormalized = temp.exitTime;

        for (const auto& condText : temp.conds)
        {
            CAnimatorController::Condition cond{};
            if (ParseCondition(condText, paramTypeLookup, cond))
                tr.conditions.push_back(cond);
        }

        if (temp.isAny)
        {
            info.anyStateTransitions.push_back(tr);
        }
        else
        {
            auto it = find_if(info.states.begin(), info.states.end(),
                [&](const CAnimatorController::State& st)
                {
                    return st.name == temp.from;
                });
            if (it != info.states.end())
                it->transitions.push_back(tr);
        }
    }

    if (!fs::exists("BinaryAssets/AnimatorControllerData"))
        fs::create_directories("BinaryAssets/AnimatorControllerData");

    auto split = CEngineString::Split(_filePath, L"/");
    wstring folder = split.size() >= 2 ? split[split.size() - 2] : L"";
    wstring fileNoExt = CEngineString::Split(split.back(), L".")[0];
    wstring saveName = folder.empty() ? fileNoExt : folder + L"_" + fileNoExt;
    wstring outPath = L"BinaryAssets/AnimatorControllerData/" + saveName + L".animcontroller";

    ofstream out(outPath, ios::binary);
    if (!out.is_open())
    {
        CDebug::LogError(L"ConvertAnimatorControllerToBinary failed - cannot open output: " + outPath);
        return E_FAIL;
    }

    _uint version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(_uint));
    WriteWString(out, info.controllerName);
    WriteWString(out, info.entryState);

    _uint paramCount = static_cast<_uint>(info.parameters.size());
    out.write(reinterpret_cast<const char*>(&paramCount), sizeof(_uint));
    for (const auto& param : info.parameters)
    {
        WriteWString(out, param.name);
        auto type = static_cast<_uint>(param.type);
        out.write(reinterpret_cast<const char*>(&type), sizeof(_uint));
        out.write(reinterpret_cast<const char*>(&param.defaultBool), sizeof(_bool));
        out.write(reinterpret_cast<const char*>(&param.defaultInt), sizeof(_int));
        out.write(reinterpret_cast<const char*>(&param.defaultFloat), sizeof(_float));
    }

    _uint stateCount = static_cast<_uint>(info.states.size());
    out.write(reinterpret_cast<const char*>(&stateCount), sizeof(_uint));
    for (const auto& st : info.states)
    {
        WriteWString(out, st.name);
        WriteWString(out, st.motionName);
        out.write(reinterpret_cast<const char*>(&st.speedMul), sizeof(_float));

        _uint trCount = static_cast<_uint>(st.transitions.size());
        out.write(reinterpret_cast<const char*>(&trCount), sizeof(_uint));
        for (const auto& tr : st.transitions)
        {
            WriteWString(out, tr.toState);
            out.write(reinterpret_cast<const char*>(&tr.blendDuration), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
            out.write(reinterpret_cast<const char*>(&tr.exitTimeNormalized), sizeof(_float));

            _uint condCount = static_cast<_uint>(tr.conditions.size());
            out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
            for (const auto& cond : tr.conditions)
            {
                WriteWString(out, cond.paramName);
                auto op = static_cast<_uint>(cond.op);
                out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
                out.write(reinterpret_cast<const char*>(&cond.b), sizeof(_bool));
                out.write(reinterpret_cast<const char*>(&cond.i), sizeof(_int));
                out.write(reinterpret_cast<const char*>(&cond.f), sizeof(_float));
            }
        }
    }

    _uint anyCount = static_cast<_uint>(info.anyStateTransitions.size());
    out.write(reinterpret_cast<const char*>(&anyCount), sizeof(_uint));
    for (const auto& tr : info.anyStateTransitions)
    {
        WriteWString(out, tr.toState);
        out.write(reinterpret_cast<const char*>(&tr.blendDuration), sizeof(_float));
        out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
        out.write(reinterpret_cast<const char*>(&tr.exitTimeNormalized), sizeof(_float));

        _uint condCount = static_cast<_uint>(tr.conditions.size());
        out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
        for (const auto& cond : tr.conditions)
        {
            WriteWString(out, cond.paramName);
            auto op = static_cast<_uint>(cond.op);
            out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
            out.write(reinterpret_cast<const char*>(&cond.b), sizeof(_bool));
            out.write(reinterpret_cast<const char*>(&cond.i), sizeof(_int));
            out.write(reinterpret_cast<const char*>(&cond.f), sizeof(_float));
        }
    }

    out.close();
    CDebug::Log(L"ConvertAnimatorControllerToBinary complete: " + outPath);

    return S_OK;
}
