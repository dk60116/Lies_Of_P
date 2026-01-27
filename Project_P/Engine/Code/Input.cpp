#include "epch.h"
#include "Input.h"

HHOOK CInput::s_mouseHook = nullptr;

CInput::CInput()
    : m_fWheelAxis(0.f)
    , m_fWheelRaw(0.f)
{
}

CInput::~CInput()
{
    Release();
}

CInput& CInput::GetInstance()
{
    static CInput inst;
    return inst;
}

HRESULT CInput::Initialize()
{
    //InstallMouseHook();

    return S_OK;
}

void CInput::Release()
{
    m_bKeyState.clear();
    m_bPrevKeyState.clear();

    //UninstallMouseHook();
}

_bool CInput::GetKey_Editor(_int _iKey)
{
    return m_bKeyState[_iKey] && IsEditor();
}

_bool CInput::GetKeyDown_Editor(_int _iKey)
{
    return m_bKeyState[_iKey] && !m_bPrevKeyState[_iKey] && IsEditor();
}

_bool CInput::GetKeyUp_Editor(_int _iKey)
{
    return !m_bKeyState[_iKey] && m_bPrevKeyState[_iKey] && IsEditor();
}

_bool CInput::GetMouseButton_Editor(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return m_bKeyState[_button] && IsEditor();
}

_bool CInput::GetMouseButtonDown_Editor(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return m_bKeyState[_button] && !m_bPrevKeyState[_button] && IsEditor();
}

_bool CInput::GetMouseButtonUp_Editor(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return !m_bKeyState[_button] && m_bPrevKeyState[_button] && IsEditor();
}

bool CInput::GetKey(_int _iKey)
{
    return m_bKeyState[_iKey] && !IsEditor();
}

bool CInput::GetKeyDown(_int _iKey)
{
    return m_bKeyState[_iKey] && !m_bPrevKeyState[_iKey] && !IsEditor();
}

bool CInput::GetKeyUp(_int _iKey)
{
    return !m_bKeyState[_iKey] && m_bPrevKeyState[_iKey] && !IsEditor();
}

bool CInput::GetMouseButton(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return m_bKeyState[_button] && !IsEditor();
}

bool CInput::GetMouseButtonDown(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return m_bKeyState[_button] &&!m_bPrevKeyState[_button] && !IsEditor();
}

bool CInput::GetMouseButtonUp(_int _button)
{
    if (_button == 0)
        _button = MOUSE_L;
    else if (_button == 1)
        _button = MOUSE_R;
    else if (_button == 2)
        _button = MOUSE_WHILL;
    else
        return false;

    return !m_bKeyState[_button] && m_bPrevKeyState[_button] && !IsEditor();
}

const vector2Int CInput::GetMousePos_Editor()
{
    POINT ptMouse;
    GetCursorPos(&ptMouse);

    ScreenToClient(CEditor::GetInstance().Get_EditorWindow(), &ptMouse);

    ptMouse.y -= CEditor::GetInstance().Get_Options().topBarHeight;

    return vector2Int((_int)ptMouse.x, (_int)ptMouse.y);
}

const vector2Int CInput::GetMousePos()
{
    POINT ptMouse;
    GetCursorPos(&ptMouse);
    ScreenToClient(CDisplay::GetInstance().Get_GameWindow(), &ptMouse);

    return vector2Int((_int)ptMouse.x, (_int)ptMouse.y);
}

const _float CInput::GetAxis_Editor(const wstring _axisName)
{
    if (!IsEditor())
        return 0.f;

    _float result = 0.f;

    if (_axisName == L"Horizontal")
    {
        if (GetKey_Editor(LEFT))
            result -= 1.f;
        if (GetKey_Editor(RIGHT))
            result += 1.f;
        if (GetKey_Editor(A))
            result -= 1.f;
        if (GetKey_Editor(D))
            result += 1.f;
    }
    else if (_axisName == L"Vertical")
    {
        if (GetKey_Editor(DOWN))
            result -= 1.f;
        if (GetKey_Editor(UP))
            result += 1.f;
        if (GetKey_Editor(S))
            result -= 1.f;
        if (GetKey_Editor(W))
            result += 1.f;
    }
    else if (_axisName == L"Mouse ScrollWheel")
        result = m_fWheelAxis;
    else
        return 0.f;

    result = clamp(result, -1.f, 1.f);

    return result;
}

const _float CInput::GetAxis(const wstring _axisName)
{
    if (IsEditor())
        return 0.f;

    _float result = 0.f;

    if (_axisName == L"Horizontal")
    {
        if (GetKey(LEFT))
            result -= 1.f;
        if (GetKey(RIGHT))
            result += 1.f;
        if (GetKey(A))
            result -= 1.f;
        if (GetKey(D))
            result += 1.f;
    }
    else if (_axisName == L"Vertical")
    {
        if (GetKey(DOWN))
            result -= 1.f;
        if (GetKey(UP))
            result += 1.f;
        if (GetKey(S))
            result -= 1.f;
        if (GetKey(W))
            result += 1.f;
    }
    else if (_axisName == L"Mouse ScrollWheel")
        result = m_fWheelAxis;
    else
        return 0.f;

    result = clamp(result, -1.f, 1.f);

    return result;
}

_float& CInput::Get_WheelAxisRaw()
{
    return m_fWheelRaw;
}

void CInput::Reset()
{
    for (auto& key : m_bKeyState)
    {
        key.second = false;
        m_bPrevKeyState[key.first] = false;
    }
}

void CInput::Update()
{
    for (auto& key : m_bKeyState)
    {
        m_bPrevKeyState[key.first] = key.second;
        key.second = (GetAsyncKeyState(key.first) & 0x8000) != 0;
    }

    if (abs(m_fWheelRaw) > 0.f)
    {
        m_fWheelAxis += m_fWheelRaw * m_sWheelOption.sensitivity;
        m_fWheelRaw = 0.f;
    }

    if (m_fWheelAxis > 0.f)
        m_fWheelAxis = max(0.f, m_fWheelAxis - m_sWheelOption.gravity * DELTA_TIME);
    else if (m_fWheelAxis < 0.f)
        m_fWheelAxis = min(0.f, m_fWheelAxis + m_sWheelOption.gravity * DELTA_TIME);

    if (abs(m_fWheelAxis) < m_sWheelOption.dead)
        m_fWheelAxis = 0.f;

    m_fWheelAxis = clamp(m_fWheelAxis, -1.f, 1.f);
}

LRESULT MouseProc(_int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        if (wParam == WM_MOUSEWHEEL)
        {
            MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            if (pMouse)
            {
                short delta = static_cast<short>(HIWORD(pMouse->mouseData));
                float normalized = static_cast<float>(delta) / WHEEL_DELTA;

                CInput::GetInstance().Get_WheelAxisRaw() += normalized;
            }
        }
    }

    return CallNextHookEx(CInput::s_mouseHook, nCode, wParam, lParam);
}

void CInput::InstallMouseHook()
{
    if (s_mouseHook == nullptr)
        s_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, nullptr, 0);
}

void CInput::UninstallMouseHook()
{
    if (s_mouseHook != nullptr)
    {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = nullptr;
    }
}

const _bool CInput::IsEditor() const
{
#ifdef _CLIENT_BUILD
    return false;
#endif

    return GetForegroundWindow() == CEditor::GetInstance().Get_EditorWindow();
}
