#include "epch.h"
#include <Jolt/Core/IssueReporting.h>

static void JoltTraceImpl(const char* fmt, ...)
{
    char buf[2048];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef _WIN32
    OutputDebugStringA(buf);
#else
    std::fputs(buf, stdout);
#endif
}

static bool JoltAssertFailedImpl(const char* expr, const char* msg, const char* file, unsigned int line)
{
    char buf[2048];
    snprintf(buf, sizeof(buf),
        "Jolt ASSERT: %s | %s (%s:%u)\n",
        expr, msg ? msg : "", file, line);

#ifdef _WIN32
    OutputDebugStringA(buf);
#else
    std::fputs(buf, stdout);
#endif

#ifdef _DEBUG
    __debugbreak();
#endif
    return true;
}

namespace JPH
{
    TraceFunction Trace = &JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    AssertFailedFunction AssertFailed = &JoltAssertFailedImpl;
#endif
}
