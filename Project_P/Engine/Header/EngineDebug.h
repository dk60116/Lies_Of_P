#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CDebug final 
{
    SINGLETONCLASS(CDebug);

public:
    static HRESULT Initialize();
    void Release();

public:
    static void Log(const char* format, ...);
    static void Log(const string format, ...);
    static void Log(const wstring& format, ...);
    static void Log(const _bool format, ...);
    static void Log(const _int format, ...);
    static void Log(const _uint format, ...);
    static void Log(const size_t format, ...);
    static void Log(const _float format, ...);
    static void Log(const vector2 format, ...);
    static void Log(const vector2Int format, ...);
    static void Log(const vector3 format, ...);
    static void Log(const _float4x4 format, ...);
    static void Log(const _matrix, ...);

    static void LogError(const char* format, ...);
    static void LogError(const string format, ...);
    static void LogError(const wstring& format, ...);
    static void LogError(const _bool format, ...);
    static void LogError(const _int format, ...);
    static void LogError(const _uint format, ...);
    static void LogError(const size_t format, ...);
    static void LogError(const _float format, ...);
    static void LogError(const vector2 format, ...);
    static void LogError(const vector2Int format, ...);
    static void LogError(const vector3 format, ...);

    static void LogWarnning(const char* format, ...);
    static void LogWarnning(const string format, ...);
    static void LogWarnning(const wstring& format, ...);
    static void LogWarnning(const _bool format, ...);
    static void LogWarnning(const _int format, ...);
    static void LogWarnning(const _uint format, ...);
    static void LogWarnning(const size_t format, ...);
    static void LogWarnning(const _float format, ...);
    static void LogWarnning(const vector2 format, ...);
    static void LogWarnning(const vector2Int format, ...);
    static void LogWarnning(const vector3 format, ...);

    static string MemoryUseLog();

private:
    static void ShowElapsedTime();
};

NS_END

