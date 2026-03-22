#include "epch.h"
#include "Display.h"

namespace
{
	void UpdateShowCursorState(const bool visible)
	{
		CURSORINFO cursorInfo = {};
		cursorInfo.cbSize = sizeof(cursorInfo);

		if (GetCursorInfo(&cursorInfo))
		{
			const bool isVisible = (cursorInfo.flags & CURSOR_SHOWING) != 0;
			if (isVisible == visible)
				return;
		}

		if (visible)
		{
			for (_int i = 0; i < 8; ++i)
			{
				if (ShowCursor(TRUE) >= 0)
					break;
			}
		}
		else
		{
			for (_int i = 0; i < 8; ++i)
			{
				if (ShowCursor(FALSE) < 0)
					break;
			}
		}
	}
}

CDisplay::CDisplay()
	: m_hInst(nullptr)
	, m_hGameWindow(nullptr)
	, m_hEditorWindow(nullptr)
	, m_bIsFullScreen(false)
	, m_iWidth(1280)
	, m_iHeight(720)
	, m_bVisibleCursor(true)
{
}

CDisplay::~CDisplay()
{
}

CDisplay& CDisplay::GetInstance()
{
	static CDisplay inst;
	return inst;
}

HRESULT CDisplay::Initialize(HINSTANCE _hInst, HWND _hGameWnd, HWND _hEditorWnd)
{
	if (!_hInst)
		return E_FAIL;
	if (!_hGameWnd)
		return E_FAIL;

	m_hInst = _hInst;
	m_hGameWindow = _hGameWnd;
	m_hEditorWindow = _hEditorWnd;

	RECT rc;
	GetClientRect(m_hGameWindow, &rc);
	ApplyCursorVisibility(m_hGameWindow);

	return S_OK;
}

HINSTANCE CDisplay::Get_HInstance() const
{
	return m_hInst;
}

HWND CDisplay::Get_GameWindow() const
{
	return m_hGameWindow;
}

HWND CDisplay::Get_EditorWindow() const
{
	return m_hEditorWindow;
}

const vector2Int CDisplay::Get_ScreenResolution() const
{
	return vector2Int(m_iWidth, m_iHeight);
}

const _float CDisplay::Get_Aspect() const
{
	return static_cast<_float>(m_iWidth) / static_cast<_float>(m_iHeight);
}

const _bool CDisplay::IsVisibleCursor() const
{
	return m_bVisibleCursor;
}

void CDisplay::SetCursorVisible(const _bool _value)
{
	if (m_bVisibleCursor == _value)
		return;

	m_bVisibleCursor = _value;
	ApplyCursorVisibility(m_hGameWindow);
}

void CDisplay::ApplyCursorVisibility(HWND _hWnd) const
{
	HWND hTargetWindow = _hWnd ? _hWnd : m_hGameWindow;
	UpdateShowCursorState(m_bVisibleCursor);

	if (!m_bVisibleCursor)
	{
		SetCursor(nullptr);

		if (hTargetWindow)
			SetClassLongPtr(hTargetWindow, GCLP_HCURSOR, 0);

		return;
	}

	HCURSOR hCursor = LoadCursor(nullptr, IDC_ARROW);
	SetCursor(hCursor);

	if (hTargetWindow)
		SetClassLongPtr(hTargetWindow, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(hCursor));
}
