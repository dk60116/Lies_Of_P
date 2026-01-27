#include "epch.h"
#include "EngineString.h"

_bool CEngineString::Contains(const string& _str, const string& _target)
{
    return _str.find(_target) != string::npos;
}

_bool CEngineString::Contains(const wstring& _wstr, const wstring& _wtarget)
{
    return _wstr.find(_wtarget) != string::npos;
}

_bool CEngineString::Contains(const string& _str, const char _target)
{
    return _str.find(_target) != string::npos;
}

_bool CEngineString::Contains(const wstring& _wstr, const wchar_t _wtarget)
{
    return _wstr.find(_wtarget) != string::npos;
}

string CEngineString::WStringToString(const wstring& _wstr)
{
    if (_wstr.empty())
        return string();

    _int size_needed = WideCharToMultiByte(CP_UTF8, 0, &_wstr[0], (int)_wstr.size(), NULL, 0, NULL, NULL);
    string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &_wstr[0], (int)_wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

wstring CEngineString::StringToWString(const string& _str)
{
    if (_str.empty())
        return wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, _str.c_str(), (int)_str.size(), NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, _str.c_str(), (int)_str.size(), &wstr[0], size_needed);

    return wstr;
}

string CEngineString::Trim(const string& _str)
{
    const char* whitespace = " \t\n\r";

    const size_t start = _str.find_first_not_of(whitespace);

    if (start == string::npos)
        return "";

    const size_t end = _str.find_last_not_of(whitespace);
    
    return _str.substr(start, end - start + 1);
}

wstring CEngineString::Trim(const wstring& _wstr)
{
    const wchar_t* whitespace = L" \t\n\r";

    const size_t start = _wstr.find_first_not_of(whitespace);

    if (start == wstring::npos)
        return L"";

    const size_t end = _wstr.find_last_not_of(whitespace);
    
    return _wstr.substr(start, end - start + 1);
}

vector<string> CEngineString::Split(const string& _str, const string& _delimiter)
{
    vector<string> tokens;

    if (_delimiter.empty())
        return tokens;

    if (_str.find(_delimiter) == string::npos)
        return tokens;

    size_t start = 0;
    size_t pos = 0;

    while ((pos = _str.find(_delimiter, start)) != string::npos)
    {
        tokens.emplace_back(_str.substr(start, pos - start));
        start = pos + _delimiter.length();
    }

    tokens.emplace_back(_str.substr(start));

    return tokens;
}

vector<wstring> CEngineString::Split(const wstring& _wstr, const wstring& _delimiter)
{
    vector<wstring> tokens;

    if (_delimiter.empty())
        return tokens;

    if (_wstr.find(_delimiter) == string::npos)
        return tokens;

    size_t start = 0;
    size_t pos = 0;

    while ((pos = _wstr.find(_delimiter, start)) != string::npos)
    {
        tokens.emplace_back(_wstr.substr(start, pos - start));
        start = pos + _delimiter.length();
    }

    tokens.emplace_back(_wstr.substr(start));

    return tokens;
}

string CEngineString::Erase(const string& _str, const string& _target)
{
    string result = _str;
    size_t pos = 0;

    if (_target.empty())
        return result;

    while ((pos = result.find(_target, pos)) != string::npos)
        result.erase(pos, _target.length());

    return result;
}

wstring CEngineString::Erase(const wstring& _wstr, const wstring& _target)
{
    wstring result = _wstr;
    size_t pos = 0;

    if (_target.empty())
        return result;

    while ((pos = result.find(_target, pos)) != string::npos)
        result.erase(pos, _target.length());

    return result;
}

string CEngineString::Replace(const string& _str, const string& _dest, const string& _target)
{
    std::string result = _str;
    size_t pos = 0;

    while ((pos = result.find(_dest, pos)) != std::string::npos)
    {
        result.replace(pos, _dest.length(), _target);
        pos += _target.length();
    }

    return result;
}

wstring CEngineString::Replace(const wstring& _wstr, const wstring& _dest, const wstring& _target)
{
    wstring result = _wstr;
    size_t pos = 0;

    while ((pos = result.find(_dest, pos)) != std::wstring::npos)
    {
        result.replace(pos, _dest.length(), _target);
        pos += _target.length();
    }

    return result;
}
