#pragma once

#include "epch.h"

class ENGINE_DLL CEngineString
{
	SINGLETONCLASS(CEngineString);

public:
	static _bool Contains(const string& _str, const string& _target);
	static _bool Contains(const wstring& _wstr, const wstring& _wtarget);
	static _bool Contains(const string& _str, const char _target);
	static _bool Contains(const wstring& _wstr, const wchar_t _wtarget);
	static string WStringToString(const wstring& _wstr);
	static wstring StringToWString(const string& _str);
	static string Trim(const string& _str);
	static wstring Trim(const wstring& _wstr);
	static vector<string> Split(const string& _str, const string& _delimiter);
	static vector<wstring> Split(const wstring& _wstr, const wstring& _delimiter);
	static string Erase(const string& _str, const string& _target);
	static wstring Erase(const wstring& _wstr, const wstring& _target);
	static string Replace(const string& _str, const string& _dest, const string& _target);
	static wstring Replace(const wstring& _wstr, const wstring& _dest, const wstring& _target);
};

