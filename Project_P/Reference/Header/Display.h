#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CDisplay final
{
	SINGLETONCLASS(CDisplay);

public:
	HRESULT Initialize(HINSTANCE _hInst, HWND _hGameWnd, HWND _hEditorWnd);

public:
	HINSTANCE Get_HInstance() const;
	HWND Get_GameWindow() const;
	HWND Get_EditorWindow() const;
	const vector2Int Get_ScreenResolution() const;
	const _float Get_Aspect() const;

private:
	HINSTANCE m_hInst;
	HWND m_hGameWindow;
	HWND m_hEditorWindow;
	_bool m_bIsFullScreen;
	_uint m_iWidth, m_iHeight;
};

NS_END

