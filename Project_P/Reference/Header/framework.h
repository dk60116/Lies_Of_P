#pragma once
      
// Windows 헤더 파일
#include <windows.h>
#include <process.h>
#include <sstream>
#include <wingdi.h>
#pragma comment(lib, "Gdi32.lib")

// DirectX 헤더 파일
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <wrl/client.h>
#include <WICTextureLoader.h>
#include <SpriteFont.h>
#include <SpriteBatch.h>
#pragma comment(lib, "d3dcompiler.lib")
using namespace DirectX;
using Microsoft::WRL::ComPtr;

#ifdef new
#undef new
#endif

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// C 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <algorithm>

#ifdef new
#undef new
#define NEED_RESTORE_NEW
#endif

#include "imgui.h"
#include "ImGuizmo.h"
#include <imgui_stdlib.h>

#ifdef NEED_RESTORE_NEW
#define new DEBUG_NEW
#undef NEED_RESTORE_NEW
#endif

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "json.hpp"
using json = nlohmann::json;

// 커스텀 헤더파일
#include "MainProcess.h"
#include <iostream> 
#include <string>
using namespace std;
