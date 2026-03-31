#include "cpch.h"
#include "PlayerCamera.h"
#include "PlayerController.h"
#include "GameManager.h"

namespace
{
    constexpr _float PI = 3.141592f;
    constexpr _float kLowestCameraLift = 0.5f;
    constexpr _float kHighestCameraZoomOutStartPitch = -10.f;
    constexpr _float kHighestCameraZoomOutAmount = 3.f;

    inline _float Deg2Rad(_float deg) { return deg * (PI / 180.f); }

    inline _float WrapDeg(_float deg)
    {
        while (deg >= 360.f) deg -= 360.f;
        while (deg < 0.f)    deg += 360.f;
        return deg;
    }

    inline POINT GetClientCenterScreen(HWND hWnd)
    {
        RECT rc = {};
        GetClientRect(hWnd, &rc);
        POINT c{};
        c.x = (rc.left + rc.right) / 2;
        c.y = (rc.top + rc.bottom) / 2;
        ClientToScreen(hWnd, &c);
        return c;
    }

    inline RECT GetClientRectScreen(HWND hWnd)
    {
        RECT rc = {};
        GetClientRect(hWnd, &rc);
        POINT lt{ rc.left, rc.top };
        POINT rb{ rc.right, rc.bottom };
        ClientToScreen(hWnd, &lt);
        ClientToScreen(hWnd, &rb);
        return RECT{ lt.x, lt.y, rb.x, rb.y };
    }

    inline HWND GetRootWindow(HWND hWnd)
    {
        if (!hWnd) 
            return nullptr;
        return GetAncestor(hWnd, GA_ROOT);
    }

    inline bool IsOurWindowActive(HWND hWnd)
    {
        if (!hWnd) 
            return false;
        HWND fg = GetForegroundWindow();
        if (!fg) 
            return false;

        return GetRootWindow(hWnd) == GetRootWindow(fg);
    }

    inline void LockCursorToClient(HWND hWnd)
    {
        RECT clip = GetClientRectScreen(hWnd);
        ClipCursor(&clip);
    }
}

CPlayerCamera::CPlayerCamera()
    : m_pCamera(nullptr)
    , m_sOptions({})
    , m_fBackOffset(6.f)
    , m_fZoomSensor(0.f)
    , m_bMouseLocked(true)
    , m_bIgnoreNextDelta(true)
    , m_fTargetYaw(0.f)
    , m_fTargetPitch(15.f)
    , m_fCurYaw(0.f)
    , m_fCurPitch(15.f)
{
}

CPlayerCamera::~CPlayerCamera()
{
}

CPlayerCamera* CPlayerCamera::Create()
{
    return new CPlayerCamera();
}

CComponent* CPlayerCamera::Clone() const
{
    return new CPlayerCamera();
}

HRESULT CPlayerCamera::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    m_pCamera = m_pGameObject->AddComponent<CCamera>();

    m_fZoomSensor = m_sOptions.firstZoomSensor;

    m_fTargetYaw = 0.f;
    m_fTargetPitch = 15.f;

    m_fCurYaw = m_fTargetYaw;
    m_fCurPitch = m_fTargetPitch;

    return S_OK;
}

void CPlayerCamera::Awake()
{
    CGameManager::GetInstance().Set_PlayerCamera(this);
}

void CPlayerCamera::Start()
{
}

void CPlayerCamera::Update()
{
    HWND hWnd = CDisplay::GetInstance().Get_GameWindow();
    if (!hWnd) return;

    if (CInput::GetInstance().GetKeyDown(ESCAPE))
    {
        m_bMouseLocked = !m_bMouseLocked;
        m_bIgnoreNextDelta = true;
    }

    const bool canUpdateMouse = IsOurWindowActive(hWnd);
    CDisplay::GetInstance().SetCursorVisible(!m_bMouseLocked || !canUpdateMouse);

    if (!canUpdateMouse)
    {
        ClipCursor(nullptr);
        m_bIgnoreNextDelta = true;
    }
    else if (m_bMouseLocked)
        LockCursorToClient(hWnd);
    else
        ClipCursor(nullptr);

    _float dx = 0.f, dy = 0.f;

    if (m_bMouseLocked && canUpdateMouse)
    {
        const POINT center = GetClientCenterScreen(hWnd);
        POINT cur{};
        GetCursorPos(&cur);

        dx = static_cast<_float>(cur.x - center.x);
        dy = static_cast<_float>(cur.y - center.y);

        SetCursorPos(center.x, center.y);

        if (m_bIgnoreNextDelta)
        {
            dx = dy = 0.f;
            m_bIgnoreNextDelta = false;
        }

        const _float hugeDelta = 20000.f;
        dx = std::clamp(dx, -hugeDelta, hugeDelta);
        dy = std::clamp(dy, -hugeDelta, hugeDelta);

        const _float sens = (m_sOptions.lookSensitivity > 0.f) ? m_sOptions.lookSensitivity : 0.15f;

        const _float maxStep = 100.f;
        _float yawStep = std::clamp(dx * sens, -maxStep, maxStep);
        _float pitchStep = std::clamp(-dy * sens, -maxStep, maxStep);

        m_fTargetYaw += yawStep;
        m_fTargetPitch += pitchStep;

        const _float pMin = (m_sOptions.pitchMin != 0.f) ? m_sOptions.pitchMin : -89.f;
        const _float pMax = (m_sOptions.pitchMax != 0.f) ? m_sOptions.pitchMax : 70.f;
        m_fTargetPitch = std::clamp(m_fTargetPitch, pMin, pMax);

        m_fTargetYaw = WrapDeg(m_fTargetYaw);
    }
    else
    {
        m_bIgnoreNextDelta = true;
    }

    CTransform* tf = GetTransform();
    CTransform* playerTf = CGameManager::GetInstance().Get_Player()->GetTransform();

    const _float speed = (m_sOptions.trackingSpeed > 0.f) ? m_sOptions.trackingSpeed : 10.f;
    const _float t = 1.f - std::exp(-speed * DELTA_TIME);

    m_fCurYaw = LerpAngle(m_fCurYaw, m_fTargetYaw, t);
    m_fCurPitch = LerpAngle(m_fCurPitch, m_fTargetPitch, t);

    const _float yawRad = Deg2Rad(m_fCurYaw);
    const _float pitchRad = Deg2Rad(m_fCurPitch);
    const _float pitchMin = (m_sOptions.pitchMin != 0.f) ? m_sOptions.pitchMin : -89.f;
    const _float zoomInStartPitch = m_sOptions.pitchZoomInStart;
    const _float zoomInMaxPitch = max(zoomInStartPitch, m_sOptions.pitchMax);
    const _float zoomInRange = zoomInMaxPitch - zoomInStartPitch;
    const _float zoomInT = zoomInRange > 0.f ? std::clamp((m_fCurPitch - zoomInStartPitch) / zoomInRange, 0.f, 1.f) : 0.f;
    const _float zoomOutRange = kHighestCameraZoomOutStartPitch - pitchMin;
    const _float zoomOutT = zoomOutRange > 0.f ? std::clamp((kHighestCameraZoomOutStartPitch - m_fCurPitch) / zoomOutRange, 0.f, 1.f) : 0.f;
    const _float effectiveBackOffset = std::clamp
    (
        m_fBackOffset - m_sOptions.pitchZoomInAmount * zoomInT + kHighestCameraZoomOutAmount * zoomOutT,
        m_sOptions.zoomMin,
        m_sOptions.zoomMax
    );
    const _float zoomRange = max(m_sOptions.zoomMax - m_sOptions.zoomMin, 0.0001f);
    const _float offsetZoomT = std::clamp((effectiveBackOffset - m_sOptions.zoomMin) / zoomRange, 0.f, 1.f);
    const _float effectiveXOffset = m_sOptions.xOffset * offsetZoomT;

    const _float cy = cos(yawRad);
    const _float sy = sin(yawRad);
    const _float cp = cos(pitchRad);
    const _float sp = sin(pitchRad);

    const vector3 playerPos = playerTf->Get_Position();
    const vector3 cameraRight = vector3(cy, 0.f, -sy).normalized();
    const vector3 pivot = playerPos + vector3::up() * m_sOptions.heightOffset + cameraRight * effectiveXOffset;

    vector3 camForward;
    camForward.x = cp * sy;
    camForward.y = sp;
    camForward.z = cp * cy;

    vector3 finalPos = pivot - camForward * effectiveBackOffset;
    finalPos.y += kLowestCameraLift * zoomInT;

    tf->Set_Position(finalPos);
    tf->LookAt(pivot);
}

void CPlayerCamera::LateUpdate()
{
    if (!CGameManager::GetInstance().Get_Player())
        return;
}

void CPlayerCamera::OnDestroy()
{
    ClipCursor(nullptr);
    CDisplay::GetInstance().SetCursorVisible(true);
}

_float CPlayerCamera::LerpAngle(_float current, _float target, _float t)
{
    _float diff = target - current;

    while (diff >= 180.f)
        diff -= 360.f;
    while (diff < -180.f)
        diff += 360.f;

    return current + diff * t;
}

CCamera* CPlayerCamera::GetCamera()
{
    return m_pCamera;
}

const vector3 CPlayerCamera::Get_ForwardVector()
{
    vector3 forward = GetTransform()->Get_Directions().forward;
    forward.y = 0.f;
    return forward.normalized();
}

const _float CPlayerCamera::Get_ForwardAngle()
{
    vector3 f = Get_ForwardVector();

    const _float len2 = f.x * f.x + f.z * f.z;
    if (len2 < 1e-6f) 
        return 0.f;

    const _float yawRad = std::atan2(f.x, f.z);
    _float yawDeg = yawRad * (180.f / PI);

    return yawDeg;
}



