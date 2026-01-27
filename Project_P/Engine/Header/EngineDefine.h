#pragma once

#ifndef Engine_Define_h__
#define Engine_Define_h__

//#define _CLIENT_BUILD

#define REGISTER_CLASS(T) \
    namespace { struct AutoRegister_##T { AutoRegister_##T() { \
        Engine::FactoryManager::GetInstance().Register(#T, []() -> Engine::UObject* { return new T(); }); \
    }} _autoReg_##T; }

#include <fstream>

//#include <fmod.h>
//#include <fmod_common.h>
//#include <fmod_errors.h>
//#include <fmod.hpp>

#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <ctime>
#include <typeindex>
#include <memory>
#include <atomic>
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "dwmapi.lib")

#include "EngineEnum.h"
#include "EngineMacro.h"

#define DIRECTINPUT_VERSION	0x0800
#include <dinput.h>

#define TRAVERSAL_ITER(p, n) auto n = p.begin(); n != p.end(); ++n

#ifndef TVM_GETHOTITEM               
#   define TVM_GETHOTITEM (TV_FIRST + 58)
#endif

#ifndef TreeView_GetHotItem
#   define TreeView_GetHotItem(hwnd) \
        (HTREEITEM)SendMessage((hwnd), TVM_GETHOTITEM, 0, 0)
#endif

#pragma warning(disable : 4251)

using namespace std;
using namespace Engine;

#endif // Engine_Define_h__
