#include "epch.h"
#include "Resources.h"
#include "AnimatorController.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
    string TrimString(const string& s)
    {
        return CEngineString::Trim(s);
    }

    vector<string> SplitString(const string& s, const string& delim)
    {
        return CEngineString::Split(s, delim);
    }

    bool ParseBoolValue(const string& s, _bool& out)
    {
        string v = TrimString(s);
        for (auto& c : v)
            c = static_cast<char>(tolower(c));

        if (v == "true" || v == "1")
        {
            out = true;
            return true;
        }
        if (v == "false" || v == "0")
        {
            out = false;
            return true;
        }
        return false;
    }

    bool TryParseCondition(const string& token,
        const unordered_map<wstring, CAnimatorController::PARAM_TYPE>& paramTypes,
        CAnimatorController::Condition& out)
    {
        string trimmed = TrimString(token);
        if (trimmed.empty())
            return false;

        static const vector<string> ops = { ">=", "<=", "==", "!=", ">", "<" };
        string opFound;
        size_t opPos = string::npos;
        for (const auto& op : ops)
        {
            opPos = trimmed.find(op);
            if (opPos != string::npos)
            {
                opFound = op;
                break;
            }
        }

        string namePart;
        string valuePart;
        if (opFound.empty())
        {
            namePart = trimmed;
        }
        else
        {
            namePart = TrimString(trimmed.substr(0, opPos));
            valuePart = TrimString(trimmed.substr(opPos + opFound.size()));
        }

        if (namePart.empty())
            return false;

        out.paramName = CEngineString::StringToWString(namePart);

        auto typeIt = paramTypes.find(out.paramName);
        CAnimatorController::PARAM_TYPE paramType = CAnimatorController::PARAM_TYPE::BOOL;
        if (typeIt != paramTypes.end())
            paramType = typeIt->second;

        using OP = CAnimatorController::COMPARE_OP;
        if (opFound == "==")
            out.op = OP::EQUAL;
        else if (opFound == "!=")
            out.op = OP::NOT_EQUAL;
        else if (opFound == ">=")
            out.op = OP::GREATER_EQUAL;
        else if (opFound == "<=")
            out.op = OP::LESS_EQUAL;
        else if (opFound == ">")
            out.op = OP::GREATER;
        else if (opFound == "<")
            out.op = OP::LESS;
        else
            out.op = OP::EQUAL;

        switch (paramType)
        {
        case CAnimatorController::PARAM_TYPE::TRIGGER:
            out.b = true;
            break;
        case CAnimatorController::PARAM_TYPE::BOOL:
            if (valuePart.empty())
            {
                out.op = OP::EQUAL;
                out.b = true;
            }
            else
            {
                _bool b = false;
                if (!ParseBoolValue(valuePart, b))
                    return false;
                out.b = b;
            }
            break;
        case CAnimatorController::PARAM_TYPE::INT:
            if (valuePart.empty())
            {
                out.op = OP::NOT_EQUAL;
                out.i = 0;
            }
            else
            {
                out.i = static_cast<_int>(atoi(valuePart.c_str()));
            }
            break;
        case CAnimatorController::PARAM_TYPE::FLOAT:
            if (valuePart.empty())
            {
                out.op = OP::NOT_EQUAL;
                out.f = 0.f;
            }
            else
            {
                out.f = static_cast<_float>(atof(valuePart.c_str()));
            }
            break;
        }

        return true;
    }
}

HRESULT CResources::ConvertAnimatorControllerToBinary(const wstring _filePath)
{
    const wstring fullPath = m_strDefaultAssetPath + _filePath;
    ifstream ifs(fullPath, ios::binary);
    if (!ifs.is_open())
    {
        CDebug::LogError(L"Failed to open AnimatorController: " + fullPath);
        return E_FAIL;
    }

    string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();

    if (text.empty())
    {
        CDebug::LogError(L"AnimatorController file is empty: " + fullPath);
        return E_FAIL;
    }

    CAnimatorController::AnimatorControllerInitInfo info{};
    unordered_map<wstring, CAnimatorController::PARAM_TYPE> paramTypes;
    unordered_map<wstring, CAnimatorController::State> stateMap;

    enum class Sec { None, Params, State, Transition, Any };
    Sec sec = Sec::None;
    wstring curStateName;
    wstring firstStateName;

    struct PendingTransition
    {
        wstring from;
        CAnimatorController::Transition tr{};
        _bool isAny = false;
    };
    PendingTransition pending{};
    _bool buildingTransition = false;

    auto flushTransition = [&]()
        {
            if (!buildingTransition)
                return;

            if (pending.isAny)
            {
                info.anyStateTransitions.push_back(pending.tr);
            }
            else
            {
                auto stIt = stateMap.find(pending.from);
                if (stIt != stateMap.end())
                    stIt->second.transitions.push_back(pending.tr);
                else
                    CDebug::LogWarnning(L"AnimatorController transition skipped - state not found: " + pending.from);
            }

            buildingTransition = false;
            pending = PendingTransition{};
        };

    istringstream iss(text);
    string line;
    while (getline(iss, line))
    {
        line = TrimString(line);
        if (line.empty())
            continue;
        if (!line.empty() && line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            flushTransition();

            string secName = TrimString(line.substr(1, line.size() - 2));
            if (secName == "parameters")
            {
                sec = Sec::Params;
                continue;
            }

            if (secName == "any")
            {
                sec = Sec::Any;
                buildingTransition = true;
                pending = PendingTransition{};
                pending.isAny = true;
                pending.tr.blendDuration = 0.15f;
                pending.tr.hasExitTime = false;
                pending.tr.exitTimeNormalized = 1.f;
                continue;
            }

            if (secName.rfind("state ", 0) == 0)
            {
                sec = Sec::State;
                string nameStr = TrimString(secName.substr(6));
                curStateName = CEngineString::StringToWString(nameStr);
                if (!curStateName.empty())
                {
                    CAnimatorController::State st{};
                    st.name = curStateName;
                    st.motionName = L"";
                    st.speedMul = 1.f;
                    stateMap[curStateName] = st;
                    if (firstStateName.empty())
                        firstStateName = curStateName;
                }
                continue;
            }

            if (secName.rfind("transition ", 0) == 0)
            {
                sec = Sec::Transition;
                string trName = TrimString(secName.substr(11));
                size_t arrow = trName.find("->");
                if (arrow == string::npos)
                {
                    sec = Sec::None;
                    continue;
                }

                string from = TrimString(trName.substr(0, arrow));
                string to = TrimString(trName.substr(arrow + 2));

                buildingTransition = true;
                pending = PendingTransition{};
                pending.from = CEngineString::StringToWString(from);
                pending.tr.toState = CEngineString::StringToWString(to);
                pending.tr.blendDuration = 0.15f;
                pending.tr.hasExitTime = false;
                pending.tr.exitTimeNormalized = 1.f;
                continue;
            }

            sec = Sec::None;
            continue;
        }

        auto eq = line.find('=');
        if (sec == Sec::None)
        {
            if (eq != string::npos)
            {
                string k = TrimString(line.substr(0, eq));
                string v = TrimString(line.substr(eq + 1));
                if (k == "name")
                    info.controllerName = CEngineString::StringToWString(v);
                else if (k == "entry")
                    info.entryState = CEngineString::StringToWString(v);
            }
            continue;
        }

        if (sec == Sec::Params)
        {
            auto parts = SplitString(line, " ");
            if (parts.size() >= 2)
            {
                string typeStr = TrimString(parts[0]);
                string rest = TrimString(line.substr(typeStr.size() + 1));

                string nameStr;
                string valueStr;
                auto eq2 = rest.find('=');
                if (eq2 == string::npos)
                {
                    nameStr = TrimString(rest);
                }
                else
                {
                    nameStr = TrimString(rest.substr(0, eq2));
                    valueStr = TrimString(rest.substr(eq2 + 1));
                }

                if (!nameStr.empty())
                {
                    CAnimatorController::ParameterDesc p{};
                    p.name = CEngineString::StringToWString(nameStr);
                    if (typeStr == "bool")
                        p.type = CAnimatorController::PARAM_TYPE::BOOL;
                    else if (typeStr == "int")
                        p.type = CAnimatorController::PARAM_TYPE::INT;
                    else if (typeStr == "float")
                        p.type = CAnimatorController::PARAM_TYPE::FLOAT;
                    else if (typeStr == "trigger")
                        p.type = CAnimatorController::PARAM_TYPE::TRIGGER;
                    else
                        p.type = CAnimatorController::PARAM_TYPE::BOOL;

                    if (!valueStr.empty())
                    {
                        if (p.type == CAnimatorController::PARAM_TYPE::BOOL)
                            ParseBoolValue(valueStr, p.defaultBool);
                        else if (p.type == CAnimatorController::PARAM_TYPE::INT)
                            p.defaultInt = static_cast<_int>(atoi(valueStr.c_str()));
                        else if (p.type == CAnimatorController::PARAM_TYPE::FLOAT)
                            p.defaultFloat = static_cast<_float>(atof(valueStr.c_str()));
                    }

                    info.parameters.push_back(p);
                    paramTypes[p.name] = p.type;
                }
            }
            continue;
        }

        if (sec == Sec::State)
        {
            if (curStateName.empty())
                continue;
            auto it = stateMap.find(curStateName);
            if (it == stateMap.end())
                continue;

            if (eq != string::npos)
            {
                string k = TrimString(line.substr(0, eq));
                string v = TrimString(line.substr(eq + 1));
                if (k == "motion")
                    it->second.motionName = CEngineString::StringToWString(v);
                else if (k == "speedMul")
                    it->second.speedMul = static_cast<_float>(atof(v.c_str()));
            }
            continue;
        }

        if ((sec == Sec::Transition || sec == Sec::Any) && buildingTransition)
        {
            if (eq == string::npos)
                continue;

            string k = TrimString(line.substr(0, eq));
            string v = TrimString(line.substr(eq + 1));

            if (k == "to")
            {
                pending.tr.toState = CEngineString::StringToWString(v);
            }
            else if (k == "blend")
            {
                pending.tr.blendDuration = static_cast<_float>(atof(v.c_str()));
            }
            else if (k == "exitTime")
            {
                pending.tr.hasExitTime = true;
                pending.tr.exitTimeNormalized = static_cast<_float>(atof(v.c_str()));
            }
            else if (k == "cond")
            {
                string conds = CEngineString::Replace(v, "&&", ",");
                auto condParts = SplitString(conds, ",");
                for (const auto& cond : condParts)
                {
                    CAnimatorController::Condition condition{};
                    if (TryParseCondition(cond, paramTypes, condition))
                        pending.tr.conditions.push_back(condition);
                }
            }
            continue;
        }
    }

    flushTransition();

    for (const auto& kv : stateMap)
        info.states.push_back(kv.second);

    if (info.controllerName.empty())
    {
        auto splitPath = CEngineString::Split(_filePath, L"/");
        if (!splitPath.empty())
        {
            auto fileNameExt = splitPath.back();
            info.controllerName = CEngineString::Split(fileNameExt, L".")[0];
        }
    }

    if (info.entryState.empty())
    {
        if (!firstStateName.empty())
            info.entryState = firstStateName;
        else if (!info.states.empty())
            info.entryState = info.states.front().name;
    }

    auto splitPath = CEngineString::Split(_filePath, L"/");
    if (splitPath.size() < 2)
    {
        CDebug::LogError(L"AnimatorController path invalid: " + _filePath);
        return E_FAIL;
    }

    wstring folder = splitPath[splitPath.size() - 2];
    wstring fileNameExt = splitPath.back();
    wstring pureName = CEngineString::Split(fileNameExt, L".")[0];
    wstring saveName = folder + L"_" + pureName;
    wstring outPath = L"BinaryAssets/AnimatorControllerData/" + saveName + L".acdata";

    if (!fs::exists("BinaryAssets/AnimatorControllerData"))
        fs::create_directories("BinaryAssets/AnimatorControllerData");

    ofstream out(outPath, ios::binary);
    if (!out.is_open())
    {
        CDebug::LogError(L"AnimatorController binary save failed: " + outPath);
        return E_FAIL;
    }

    auto writeWString = [&out](const wstring& ws)
        {
            _uint len = static_cast<_uint>(ws.size());
            out.write(reinterpret_cast<const char*>(&len), sizeof(_uint));
            if (len)
                out.write(reinterpret_cast<const char*>(ws.data()), sizeof(wchar_t) * len);
        };

    const _uint magic = 0x41434231; // "ACB1"
    out.write(reinterpret_cast<const char*>(&magic), sizeof(_uint));

    writeWString(info.controllerName);
    writeWString(info.entryState);

    _uint paramCount = static_cast<_uint>(info.parameters.size());
    out.write(reinterpret_cast<const char*>(&paramCount), sizeof(_uint));
    for (const auto& p : info.parameters)
    {
        writeWString(p.name);
        _uint type = static_cast<_uint>(p.type);
        out.write(reinterpret_cast<const char*>(&type), sizeof(_uint));
        out.write(reinterpret_cast<const char*>(&p.defaultBool), sizeof(_bool));
        out.write(reinterpret_cast<const char*>(&p.defaultInt), sizeof(_int));
        out.write(reinterpret_cast<const char*>(&p.defaultFloat), sizeof(_float));
    }

    _uint stateCount = static_cast<_uint>(info.states.size());
    out.write(reinterpret_cast<const char*>(&stateCount), sizeof(_uint));
    for (const auto& st : info.states)
    {
        writeWString(st.name);
        writeWString(st.motionName);
        out.write(reinterpret_cast<const char*>(&st.speedMul), sizeof(_float));

        _uint trCount = static_cast<_uint>(st.transitions.size());
        out.write(reinterpret_cast<const char*>(&trCount), sizeof(_uint));
        for (const auto& tr : st.transitions)
        {
            writeWString(tr.toState);
            out.write(reinterpret_cast<const char*>(&tr.blendDuration), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
            out.write(reinterpret_cast<const char*>(&tr.exitTimeNormalized), sizeof(_float));

            _uint condCount = static_cast<_uint>(tr.conditions.size());
            out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
            for (const auto& c : tr.conditions)
            {
                writeWString(c.paramName);
                _uint op = static_cast<_uint>(c.op);
                out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
                out.write(reinterpret_cast<const char*>(&c.b), sizeof(_bool));
                out.write(reinterpret_cast<const char*>(&c.i), sizeof(_int));
                out.write(reinterpret_cast<const char*>(&c.f), sizeof(_float));
            }
        }
    }

    _uint anyCount = static_cast<_uint>(info.anyStateTransitions.size());
    out.write(reinterpret_cast<const char*>(&anyCount), sizeof(_uint));
    for (const auto& tr : info.anyStateTransitions)
    {
        writeWString(tr.toState);
        out.write(reinterpret_cast<const char*>(&tr.blendDuration), sizeof(_float));
        out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
        out.write(reinterpret_cast<const char*>(&tr.exitTimeNormalized), sizeof(_float));

        _uint condCount = static_cast<_uint>(tr.conditions.size());
        out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
        for (const auto& c : tr.conditions)
        {
            writeWString(c.paramName);
            _uint op = static_cast<_uint>(c.op);
            out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
            out.write(reinterpret_cast<const char*>(&c.b), sizeof(_bool));
            out.write(reinterpret_cast<const char*>(&c.i), sizeof(_int));
            out.write(reinterpret_cast<const char*>(&c.f), sizeof(_float));
        }
    }

    out.close();
    CDebug::Log(L"Complete create animator controller data: " + _filePath);

    return S_OK;
}
