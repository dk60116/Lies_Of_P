#include "epch.h"
#include "Display.h"

CDisplay::CDisplay()
	: m_hInst(nullptr)
	, m_hGameWindow(nullptr)
	, m_hEditorWindow(nullptr)
	, m_bIsFullScreen(false)
	, m_iWidth(1280)
	, m_iHeight(720)
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
