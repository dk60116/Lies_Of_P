#include "epch.h"
#include "EngineDebug.h"
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <psapi.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

queue<string>         gLogQueue;
mutex                 gLogMtx;
condition_variable    gLogCv;
atomic_bool           gLoggerRun{ true };
thread                gLoggerThread;

void LoggerThreadMain()
{
    while (gLoggerRun)
    {
        unique_lock<mutex> lk(gLogMtx);
        gLogCv.wait(lk, [] { return !gLogQueue.empty() || !gLoggerRun; });

        while (!gLogQueue.empty())
        {
            string msg = move(gLogQueue.front());
            gLogQueue.pop();
            lk.unlock();               
            printf("%s\n", msg.c_str());
            lk.lock();
        }
    }
}

CDebug::CDebug()
{
}

CDebug::~CDebug()
{
    Release();
}

CDebug& CDebug::GetInstance()
{
    static CDebug inst;
    return inst;
}

HRESULT CDebug::Initialize()
{
#ifdef _CLIENT_BUILD
    return S_OK;
#endif

    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    gLoggerThread = thread(LoggerThreadMain);

    Log("Engine Started");
    return S_OK;
}

void CDebug::Release()
{
    gLoggerRun = false;
    gLogCv.notify_all();

    if (gLoggerThread.joinable())
        gLoggerThread.join();

    fclose(stdout);
    fclose(stderr);

    FreeConsole();
}

void CDebug::Log(const char* format, ...)
{
    ShowElapsedTime();

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void CDebug::Log(const string format, ...)
{
    ShowElapsedTime();

    va_list args;
    va_start(args, format);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);

    va_end(args);

    printf("%s\n", buffer);
    fflush(stdout);
}

void CDebug::Log(const wstring& format, ...)
{
    Log(CEngineString::WStringToString(format));
}

void CDebug::Log(const _bool format, ...)
{
	Log(format ? "True" : "False");
}

void CDebug::Log(const _int format, ...)
{
    Log(to_string(format));
}

void CDebug::Log(const _uint format, ...)
{
    Log(to_string(format));
}

void CDebug::Log(const size_t format, ...)
{
    Log(to_string(format));
}

void CDebug::Log(const _float format, ...)
{
    Log(to_string(format));
}

void CDebug::Log(const vector2 format, ...)
{
    Log("vector2(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::Log(const vector2Int format, ...)
{
    Log("vector2Int(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::Log(const vector3 format, ...)
{
    Log("vector3(" + to_string(format.x) + ", " + to_string(format.y) + ", " + to_string(format.z) + ')');
}

void CDebug::Log(const _float4x4 format, ...)
{
    char buf[512];
    sprintf_s
    (
        buf,
        "%s\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢",
        "Matrix",
        format._11, format._12, format._13, format._14,
        format._21, format._22, format._23, format._24,
        format._31, format._32, format._33, format._34,
        format._41, format._42, format._43, format._44
    );

    Log("%s", buf);
}

void CDebug::Log(const _matrix format, ...)
{
    char buf[512];

    _float4x4 float44 = {};
    XMStoreFloat4x4(&float44, format);

    sprintf_s
    (
        buf,
        "%s\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢\n"
        "¦¢ %8.3f %8.3f %8.3f %8.3f ¦¢",
        "Matrix",
        float44._11, float44._12, float44._13, float44._14,
        float44._21, float44._22, float44._23, float44._24,
        float44._31, float44._32, float44._33, float44._34,
        float44._41, float44._42, float44._43, float44._44
    );

    Log("%s", buf);
}

void CDebug::LogError(const char* format, ...)
{
    ShowElapsedTime();

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD saved_attributes = consoleInfo.wAttributes;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);

    SetConsoleTextAttribute(hConsole, saved_attributes);
}

void CDebug::LogError(const string format, ...)
{
    ShowElapsedTime();

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD saved_attributes = consoleInfo.wAttributes;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);

    va_list args;
    va_start(args, format);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);

    va_end(args);

    printf("%s\n", buffer);
    fflush(stdout);

    SetConsoleTextAttribute(hConsole, saved_attributes);
}

void CDebug::LogError(const wstring& format, ...)
{
    LogError(CEngineString::WStringToString(format));
}

void CDebug::LogError(const _bool format, ...)
{
	LogError(format ? "True" : "False");    
}

void CDebug::LogError(const _int format, ...)
{
    LogError(to_string(format));
}

void CDebug::LogError(const _uint format, ...)
{
    LogError(to_string(format));
}

void CDebug::LogError(const size_t format, ...)
{
    LogError(to_string(format));
}

void CDebug::LogError(const _float format, ...)
{
    LogError(to_string(format));
}

void CDebug::LogError(const vector2 format, ...)
{
    LogError("vector2(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::LogError(const vector2Int format, ...)
{
    LogError("vector2Int(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::LogError(const vector3 format, ...)
{
    LogError("vector3(" + to_string(format.x) + ", " + to_string(format.y) + ", " + to_string(format.z) + ')');
}

void CDebug::LogWarnning(const char* format, ...)
{
    ShowElapsedTime();

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD saved_attributes = consoleInfo.wAttributes;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);

    SetConsoleTextAttribute(hConsole, saved_attributes);
}

void CDebug::LogWarnning(const string format, ...)
{
    ShowElapsedTime();

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD saved_attributes = consoleInfo.wAttributes;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    va_list args;
    va_start(args, format);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);

    va_end(args);

    printf("%s\n", buffer);
    fflush(stdout);

    SetConsoleTextAttribute(hConsole, saved_attributes);
}

void CDebug::LogWarnning(const wstring& format, ...)
{
    LogWarnning(CEngineString::WStringToString(format));
}

void CDebug::LogWarnning(const _bool format, ...)
{
    LogWarnning(format ? "True" : "False");
}

void CDebug::LogWarnning(const _int format, ...)
{
    LogWarnning(to_string(format));
}

void CDebug::LogWarnning(const _uint format, ...)
{
    LogWarnning(to_string(format));
}

void CDebug::LogWarnning(const size_t format, ...)
{
    LogWarnning(to_string(format));
}

void CDebug::LogWarnning(const _float format, ...)
{
    LogWarnning(to_string(format));
}

void CDebug::LogWarnning(const vector2 format, ...)
{
    LogWarnning("vector2(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::LogWarnning(const vector2Int format, ...)
{
    LogWarnning("vector2Int(" + to_string(format.x) + ", " + to_string(format.y) + ')');
}

void CDebug::LogWarnning(const vector3 format, ...)
{
    LogWarnning("vector3(" + to_string(format.x) + ", " + to_string(format.y) + ", " + to_string(format.z) + ')');
}

string CDebug::MemoryUseLog()
{
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    HANDLE hProcess = GetCurrentProcess();

    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    {
        SIZE_T memUsageMB = pmc.WorkingSetSize / (1024 * 1024);
        SIZE_T privateMB = pmc.PrivateUsage / (1024 * 1024);

        return "[Using Memory] WorkingSet: " + to_string(memUsageMB) + " MB, PrivateUsage: " + to_string(privateMB) + " MB";
    }
    else
    {
        return "GetProcessMemoryInfo failed.";
    }

    return "";
}

void CDebug::ShowElapsedTime()
{
    _float elaspedTime = CTime::GetInstance().Get_ElaspedTime();
    cout << fixed << setprecision(2);
    cout << '[' << elaspedTime << ']' << ' ';
}

//CDebug::CDebug() {}
//CDebug::~CDebug() {}
//CDebug& CDebug::GetInstance() { static CDebug inst; return inst; }
//HRESULT CDebug::Initialize() { return S_OK; }
//void CDebug::Release() {}
//void CDebug::Log(const char*, ...) {}
//void CDebug::Log(const std::string format, ...) {}
//void CDebug::Log(const wstring format, ...) {}
//void CDebug::Log(const int foramt, ...) {}
//void CDebug::Log(const float format, ...) {}
//void CDebug::Log(const vector3 format, ...) {}
//void CDebug::Log(const vector2Int format, ...) {}
//void CDebug::LogError(const char* format, ...) {}
//void CDebug::LogError(const string format, ...) {}
//void CDebug::LogError(const wstring format, ...) {}
//void CDebug::LogError(const int format, ...) {}
//void CDebug::LogError(const float format, ...) {}
//void CDebug::LogError(const vector2 format, ...) {}
//void CDebug::LogError(const vector2Int format, ...) {}
//void CDebug::LogError(const vector3 format, ...) {}
//string CDebug::MemoryUseLog() { return ""; }
