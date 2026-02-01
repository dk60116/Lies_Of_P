#include "epch.h"
#include "Resources.h"

#include <optional>
#include <sstream>

namespace
{
	struct AnimatorControllerParam
	{
		string name;
		CAnimatorController::PARAM_TYPE type = CAnimatorController::PARAM_TYPE::BOOL;
		_bool defaultBool = false;
		_int defaultInt = 0;
		_float defaultFloat = 0.f;
	};

	struct AnimatorControllerCondition
	{
		string paramName;
		CAnimatorController::COMPARE_OP op = CAnimatorController::COMPARE_OP::EQUAL;
		_bool b = false;
		_int i = 0;
		_float f = 0.f;
	};

	struct AnimatorControllerTransition
	{
		string from;
		string to;
		_float blend = 0.15f;
		_bool hasExitTime = false;
		_float exitTime = 1.f;
		string condRaw;
		vector<AnimatorControllerCondition> conditions;
		_bool isAny = false;
	};

	struct AnimatorControllerState
	{
		string name;
		string motion;
		_float speedMul = 1.f;
		vector<AnimatorControllerTransition> transitions;
	};

	struct AnimatorControllerParsed
	{
		string controllerName;
		string entryState;
		vector<AnimatorControllerParam> params;
		vector<AnimatorControllerState> states;
		vector<AnimatorControllerTransition> anyTransitions;
	};

	static string Trim(const string& s)
	{
		size_t b = 0;
		while (b < s.size() && isspace(static_cast<unsigned char>(s[b])))
			++b;
		size_t e = s.size();
		while (e > b && isspace(static_cast<unsigned char>(s[e - 1])))
			--e;
		return s.substr(b, e - b);
	}

	static _bool StartsWith(const string& s, const char* prefix)
	{
		size_t n = strlen(prefix);
		if (s.size() < n)
			return false;
		return equal(prefix, prefix + n, s.begin());
	}

	static vector<string> Split(const string& s, char delim)
	{
		vector<string> out;
		stringstream ss(s);
		string tok;
		while (getline(ss, tok, delim))
			out.push_back(tok);
		return out;
	}

	static vector<string> SplitByDoubleAmp(const string& s)
	{
		vector<string> out;
		size_t start = 0;
		while (start < s.size())
		{
			size_t pos = s.find("&&", start);
			if (pos == string::npos)
			{
				out.push_back(Trim(s.substr(start)));
				break;
			}
			out.push_back(Trim(s.substr(start, pos - start)));
			start = pos + 2;
		}
		return out;
	}

	static _bool ParseBool(const string& s, _bool& out)
	{
		string lower = CEngineString::ToLowerCopy(s);
		if (lower == "true" || lower == "1")
		{
			out = true;
			return true;
		}
		if (lower == "false" || lower == "0")
		{
			out = false;
			return true;
		}
		return false;
	}

	static std::optional<CAnimatorController::COMPARE_OP> ParseCompareOp(const string& op)
	{
		using OP = CAnimatorController::COMPARE_OP;
		if (op == "==") return OP::EQUAL;
		if (op == "!=") return OP::NOT_EQUAL;
		if (op == ">") return OP::GREATER;
		if (op == ">=") return OP::GREATER_EQUAL;
		if (op == "<") return OP::LESS;
		if (op == "<=") return OP::LESS_EQUAL;
		return std::nullopt;
	}

	static void WriteWString(ofstream& out, const wstring& value)
	{
		_uint len = static_cast<_uint>(value.size());
		out.write(reinterpret_cast<const char*>(&len), sizeof(_uint));
		if (len > 0)
			out.write(reinterpret_cast<const char*>(value.data()), sizeof(wchar_t) * len);
	}

	static void ParseConditions(const string& condRaw,
		const unordered_map<string, AnimatorControllerParam>& params,
		vector<AnimatorControllerCondition>& outConditions)
	{
		if (condRaw.empty())
			return;

		vector<string> parts = SplitByDoubleAmp(condRaw);
		for (const string& rawPart : parts)
		{
			string part = Trim(rawPart);
			if (part.empty())
				continue;

			string opToken;
			size_t opPos = string::npos;
			const array<string, 6> ops = { "==", "!=", ">=", "<=", ">", "<" };
			for (const auto& op : ops)
			{
				opPos = part.find(op);
				if (opPos != string::npos)
				{
					opToken = op;
					break;
				}
			}

			AnimatorControllerCondition cond{};
			if (opPos == string::npos)
			{
				cond.paramName = Trim(part);
				cond.op = CAnimatorController::COMPARE_OP::EQUAL;
			}
			else
			{
				string left = Trim(part.substr(0, opPos));
				string right = Trim(part.substr(opPos + opToken.size()));
				cond.paramName = left;
				auto parsedOp = ParseCompareOp(opToken);
				if (!parsedOp.has_value())
				{
					CDebug::LogError("AnimatorController condition has invalid operator: " + part);
					continue;
				}
				cond.op = parsedOp.value();

				auto pit = params.find(left);
				if (pit == params.end())
				{
					CDebug::LogError("AnimatorController condition param not found: " + left);
					continue;
				}

				const auto& param = pit->second;
				if (param.type == CAnimatorController::PARAM_TYPE::BOOL)
				{
					_bool b = false;
					if (!ParseBool(right, b))
					{
						CDebug::LogError("AnimatorController condition bool parse failed: " + part);
						continue;
					}
					cond.b = b;
				}
				else if (param.type == CAnimatorController::PARAM_TYPE::INT)
				{
					cond.i = atoi(right.c_str());
				}
				else if (param.type == CAnimatorController::PARAM_TYPE::FLOAT)
				{
					cond.f = static_cast<_float>(atof(right.c_str()));
				}
			}

			auto pit = params.find(cond.paramName);
			if (pit == params.end())
			{
				CDebug::LogError("AnimatorController condition param not found: " + cond.paramName);
				continue;
			}
			if (pit->second.type == CAnimatorController::PARAM_TYPE::TRIGGER)
			{
				cond.op = CAnimatorController::COMPARE_OP::EQUAL;
			}

			outConditions.push_back(cond);
		}
	}

	static _bool ParseAnimatorControllerText(const string& text, AnimatorControllerParsed& outParsed)
	{
		outParsed = AnimatorControllerParsed{};

		enum class Sec { None, Params, State, Transition, Any };
		Sec sec = Sec::None;
		string curStateName;
		string firstStateName;
		AnimatorControllerTransition currentTransition{};
		_bool buildingTransition = false;

		unordered_map<string, size_t> stateIndex;

		auto flushTransition = [&]()
		{
			if (!buildingTransition)
				return;

			if (currentTransition.isAny)
			{
				outParsed.anyTransitions.push_back(currentTransition);
			}
			else
			{
				auto it = stateIndex.find(currentTransition.from);
				if (it != stateIndex.end())
					outParsed.states[it->second].transitions.push_back(currentTransition);
				else
					CDebug::LogError("AnimatorController transition source state not found: " + currentTransition.from);
			}

			currentTransition = AnimatorControllerTransition{};
			buildingTransition = false;
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
					sec = Sec::Params;
					continue;
				}

				if (secName == "any")
				{
					sec = Sec::Any;
					buildingTransition = true;
					currentTransition = AnimatorControllerTransition{};
					currentTransition.isAny = true;
					currentTransition.blend = 0.15f;
					currentTransition.hasExitTime = false;
					currentTransition.exitTime = 1.f;
					continue;
				}

				if (StartsWith(secName, "state "))
				{
					sec = Sec::State;
					curStateName = Trim(secName.substr(6));
					if (!curStateName.empty())
					{
						if (stateIndex.find(curStateName) == stateIndex.end())
						{
							AnimatorControllerState st{};
							st.name = curStateName;
							outParsed.states.push_back(st);
							stateIndex[curStateName] = outParsed.states.size() - 1;
							if (firstStateName.empty())
								firstStateName = curStateName;
						}
					}
					continue;
				}

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
					currentTransition = AnimatorControllerTransition{};
					currentTransition.from = from;
					currentTransition.to = to;
					currentTransition.isAny = false;
					currentTransition.blend = 0.15f;
					currentTransition.hasExitTime = false;
					currentTransition.exitTime = 1.f;
					currentTransition.condRaw.clear();
					continue;
				}

				sec = Sec::None;
				continue;
			}

			size_t eq = line.find('=');
			if (sec == Sec::None)
			{
				if (eq != string::npos)
				{
					string k = Trim(line.substr(0, eq));
					string v = Trim(line.substr(eq + 1));
					if (k == "name")
						outParsed.controllerName = v;
					else if (k == "entry")
						outParsed.entryState = v;
				}
				continue;
			}

			if (sec == Sec::Params)
			{
				auto parts = Split(line, ' ');
				if (parts.size() >= 2)
				{
					string type = Trim(parts[0]);
					string rest = Trim(line.substr(type.size() + 1));
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

					AnimatorControllerParam param{};
					param.name = name;
					if (type == "bool")
					{
						param.type = CAnimatorController::PARAM_TYPE::BOOL;
						ParseBool(value, param.defaultBool);
					}
					else if (type == "int")
					{
						param.type = CAnimatorController::PARAM_TYPE::INT;
						param.defaultInt = atoi(value.c_str());
					}
					else if (type == "float")
					{
						param.type = CAnimatorController::PARAM_TYPE::FLOAT;
						param.defaultFloat = static_cast<_float>(atof(value.c_str()));
					}
					else if (type == "trigger")
					{
						param.type = CAnimatorController::PARAM_TYPE::TRIGGER;
					}

					if (!param.name.empty())
						outParsed.params.push_back(param);
				}
				continue;
			}

			if (sec == Sec::State)
			{
				if (curStateName.empty())
					continue;
				auto it = stateIndex.find(curStateName);
				if (it == stateIndex.end())
					continue;
				AnimatorControllerState& st = outParsed.states[it->second];

				if (eq != string::npos)
				{
					string k = Trim(line.substr(0, eq));
					string v = Trim(line.substr(eq + 1));
					if (k == "motion")
						st.motion = v;
					else if (k == "speedMul")
						st.speedMul = static_cast<_float>(atof(v.c_str()));
				}
				continue;
			}

			if ((sec == Sec::Transition || sec == Sec::Any) && buildingTransition)
			{
				if (eq == string::npos)
					continue;
				string k = Trim(line.substr(0, eq));
				string v = Trim(line.substr(eq + 1));

				if (k == "to")
					currentTransition.to = v;
				else if (k == "blend")
					currentTransition.blend = static_cast<_float>(atof(v.c_str()));
				else if (k == "exitTime")
				{
					currentTransition.hasExitTime = true;
					currentTransition.exitTime = static_cast<_float>(atof(v.c_str()));
				}
				else if (k == "cond")
					currentTransition.condRaw = v;
				continue;
			}
		}

		flushTransition();

		if (outParsed.entryState.empty())
		{
			if (!firstStateName.empty())
				outParsed.entryState = firstStateName;
			else if (!outParsed.states.empty())
				outParsed.entryState = outParsed.states.front().name;
		}

		return true;
	}
}

HRESULT CResources::ConvertAnimatorControllerToBinary(const wstring _filePath)
{
	const wstring fullPath = GetInstance().m_strDefaultAssetPath + _filePath;
	ifstream in(fullPath, ios::binary);
	if (!in.is_open())
	{
		CDebug::LogError(L"AnimatorController binary build failed - cannot open: " + fullPath);
		return E_FAIL;
	}

	string text((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
	in.close();

	if (text.empty())
	{
		CDebug::LogError(L"AnimatorController binary build failed - empty file: " + fullPath);
		return E_FAIL;
	}

	AnimatorControllerParsed parsed{};
	if (!ParseAnimatorControllerText(text, parsed))
	{
		CDebug::LogError(L"AnimatorController binary build failed - parse error: " + fullPath);
		return E_FAIL;
	}

	unordered_map<string, AnimatorControllerParam> paramLookup;
	paramLookup.reserve(parsed.params.size());
	for (const auto& param : parsed.params)
		paramLookup[param.name] = param;

	for (auto& state : parsed.states)
	{
		for (auto& tr : state.transitions)
			ParseConditions(tr.condRaw, paramLookup, tr.conditions);
	}

	for (auto& tr : parsed.anyTransitions)
		ParseConditions(tr.condRaw, paramLookup, tr.conditions);

	wstring controllerName = parsed.controllerName.empty()
		? CEngineString::Split(_filePath, L"/").back()
		: CEngineString::StringToWString(parsed.controllerName);

	if (parsed.controllerName.empty())
	{
		auto parts = CEngineString::Split(controllerName, L".");
		if (!parts.empty())
			controllerName = parts[0];
	}

	auto splitPath = CEngineString::Split(_filePath, L"/");
	wstring fileNoExt = CEngineString::Split(splitPath.back(), L".")[0];
	wstring saveName = fileNoExt;
	if (splitPath.size() >= 2)
	{
		wstring folder = splitPath[splitPath.size() - 2];
		saveName = folder + L"_" + fileNoExt;
	}

	fs::create_directories("BinaryAssets/AnimatorControllerData");
	wstring outPath = L"BinaryAssets/AnimatorControllerData/" + saveName + L".animcontrollerdata";
	ofstream out(outPath, ios::binary);
	if (!out.is_open())
	{
		CDebug::LogError(L"AnimatorController binary build failed - cannot save: " + outPath);
		return E_FAIL;
	}

	_uint version = 1;
	out.write(reinterpret_cast<const char*>(&version), sizeof(_uint));

	WriteWString(out, controllerName);
	WriteWString(out, CEngineString::StringToWString(parsed.entryState));

	_uint paramCount = static_cast<_uint>(parsed.params.size());
	out.write(reinterpret_cast<const char*>(&paramCount), sizeof(_uint));
	for (const auto& param : parsed.params)
	{
		_uint type = static_cast<_uint>(param.type);
		out.write(reinterpret_cast<const char*>(&type), sizeof(_uint));
		WriteWString(out, CEngineString::StringToWString(param.name));
		out.write(reinterpret_cast<const char*>(&param.defaultBool), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&param.defaultInt), sizeof(_int));
		out.write(reinterpret_cast<const char*>(&param.defaultFloat), sizeof(_float));
	}

	_uint stateCount = static_cast<_uint>(parsed.states.size());
	out.write(reinterpret_cast<const char*>(&stateCount), sizeof(_uint));
	for (const auto& state : parsed.states)
	{
		WriteWString(out, CEngineString::StringToWString(state.name));
		WriteWString(out, CEngineString::StringToWString(state.motion));
		out.write(reinterpret_cast<const char*>(&state.speedMul), sizeof(_float));

		_uint trCount = static_cast<_uint>(state.transitions.size());
		out.write(reinterpret_cast<const char*>(&trCount), sizeof(_uint));
		for (const auto& tr : state.transitions)
		{
			WriteWString(out, CEngineString::StringToWString(tr.to));
			out.write(reinterpret_cast<const char*>(&tr.blend), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&tr.exitTime), sizeof(_float));

			_uint condCount = static_cast<_uint>(tr.conditions.size());
			out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
			for (const auto& cond : tr.conditions)
			{
				WriteWString(out, CEngineString::StringToWString(cond.paramName));
				_uint op = static_cast<_uint>(cond.op);
				out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
				out.write(reinterpret_cast<const char*>(&cond.b), sizeof(_bool));
				out.write(reinterpret_cast<const char*>(&cond.i), sizeof(_int));
				out.write(reinterpret_cast<const char*>(&cond.f), sizeof(_float));
			}
		}
	}

	_uint anyCount = static_cast<_uint>(parsed.anyTransitions.size());
	out.write(reinterpret_cast<const char*>(&anyCount), sizeof(_uint));
	for (const auto& tr : parsed.anyTransitions)
	{
		WriteWString(out, CEngineString::StringToWString(tr.to));
		out.write(reinterpret_cast<const char*>(&tr.blend), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&tr.hasExitTime), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&tr.exitTime), sizeof(_float));

		_uint condCount = static_cast<_uint>(tr.conditions.size());
		out.write(reinterpret_cast<const char*>(&condCount), sizeof(_uint));
		for (const auto& cond : tr.conditions)
		{
			WriteWString(out, CEngineString::StringToWString(cond.paramName));
			_uint op = static_cast<_uint>(cond.op);
			out.write(reinterpret_cast<const char*>(&op), sizeof(_uint));
			out.write(reinterpret_cast<const char*>(&cond.b), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&cond.i), sizeof(_int));
			out.write(reinterpret_cast<const char*>(&cond.f), sizeof(_float));
		}
	}

	out.close();

	CDebug::Log(L"AnimatorController binary build complete: " + outPath);

	return S_OK;
}
