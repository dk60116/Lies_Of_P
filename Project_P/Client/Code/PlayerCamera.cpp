#include "cpch.h"
#include "PlayerCamera.h"
#include "PlayerController.h" // CPlayer 헤더가 필요하다면 포함
#include "GameManager.h"      // GameManager, Display, Input 등 포함 가정

#include <algorithm>
#include <cmath>

namespace
{
    constexpr _float PI = 3.141592f;

    inline _float Deg2Rad(_float deg) { return deg * (PI / 180.f); }

    // 각도를 0 ~ 360으로 정규화
    inline _float WrapDeg(_float deg)
    {
        while (deg >= 360.f) deg -= 360.f;
        while (deg < 0.f)    deg += 360.f;
        return deg;
    }

    // --- Win32 Helper Functions (기존과 동일) ---
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
        if (!hWnd) return nullptr;
        return GetAncestor(hWnd, GA_ROOT);
    }

    inline bool IsOurWindowActive(HWND hWnd)
    {
        if (!hWnd) return false;
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        return GetRootWindow(hWnd) == GetRootWindow(fg);
    }

    inline void LockCursorToClient(HWND hWnd)
    {
        RECT clip = GetClientRectScreen(hWnd);
        ClipCursor(&clip);
    }
}

CPlayerCamera::CPlayerCamera()
    : m_pPlayer(nullptr)
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
    return new CPlayerCamera(); // 복사 로직 필요 시 수정
}

HRESULT CPlayerCamera::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    m_pGameObject->AddComponent<CCamera>();

    m_fZoomSensor = m_sOptions.firstZoomSensor;

    // 초기 각도 설정
    m_fTargetYaw = 0.f;
    m_fTargetPitch = 15.f;

    // 현재 각도를 타겟과 일치시켜 시작 시 튀는 현상 방지
    m_fCurYaw = m_fTargetYaw;
    m_fCurPitch = m_fTargetPitch;

    return S_OK;
}

void CPlayerCamera::Awake()
{
    // 싱글톤 접근 방식은 프로젝트 구조에 맞게 유지
    CGameManager::GetInstance().Set_PlayerCamera(this);
    m_pPlayer = CGameManager::GetInstance().Get_Player();
}

void CPlayerCamera::Start()
{
}

void CPlayerCamera::Update()
{
    HWND hWnd = CDisplay::GetInstance().Get_GameWindow();
    if (!hWnd) return;

    if (!IsOurWindowActive(hWnd))
    {
        ClipCursor(nullptr);
        m_bIgnoreNextDelta = true;
        return;
    }

    if (m_bMouseLocked)
        LockCursorToClient(hWnd);
    else
        ClipCursor(nullptr);

    _float dx = 0.f, dy = 0.f;

    if (m_bMouseLocked)
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

        // 안전 장치: 너무 큰 델타 무시
        const _float hugeDelta = 20000.f;
        dx = std::clamp(dx, -hugeDelta, hugeDelta);
        dy = std::clamp(dy, -hugeDelta, hugeDelta);

        const _float sens = (m_sOptions.lookSensitivity > 0.f) ? m_sOptions.lookSensitivity : 0.15f;

        // 프레임당 회전량 제한 (sin/cos 겹침 방지용 안전장치)
        const _float maxStep = 100.f;
        _float yawStep = std::clamp(dx * sens, -maxStep, maxStep);
        _float pitchStep = std::clamp(-dy * sens, -maxStep, maxStep);

        // 1. 입력을 Target 변수에 누적 (Wrap은 나중에 처리)
        m_fTargetYaw += yawStep;
        m_fTargetPitch += pitchStep;

        // Pitch Clamp
        const _float pMin = (m_sOptions.pitchMin != 0.f) ? m_sOptions.pitchMin : -35.f;
        const _float pMax = (m_sOptions.pitchMax != 0.f) ? m_sOptions.pitchMax : 70.f;
        m_fTargetPitch = std::clamp(m_fTargetPitch, pMin, pMax);

        // Yaw Wrap (선택 사항이나, 숫자가 무한히 커지는 것 방지)
        m_fTargetYaw = WrapDeg(m_fTargetYaw);
    }
    else
    {
        m_bIgnoreNextDelta = true;
    }

    // 줌 처리
    _float wheel = CInput::GetInstance().GetAxis(L"Mouse ScrollWheel");
    if (wheel != 0.f)
    {
        m_fBackOffset -= wheel * m_fZoomSensor * DELTA_TIME;
        m_fBackOffset = std::clamp(m_fBackOffset, m_sOptions.zoomMin, m_sOptions.zoomMax);
    }
}

// [핵심] 최단 각도 보간 함수
_float CPlayerCamera::LerpAngle(_float current, _float target, _float t)
{
    _float diff = target - current;

    // -180 ~ 180도로 보정 (예: 350도에서 10도로 갈 때 -340도가 아니라 +20도로 계산)
    while (diff >= 180.f) diff -= 360.f;
    while (diff < -180.f) diff += 360.f;

    return current + diff * t;
}

void CPlayerCamera::LateUpdate()
{
    if (!m_pPlayer) return;

    CTransform* tf = Get_Transform();
    CTransform* playerTf = m_pPlayer->Get_Transform();

    // Pivot 계산
    const vector3 playerPos = playerTf->Get_Position();
    const vector3 pivot = playerPos + vector3::up() * m_sOptions.lookHeightOffset;

    // 1. 각도(Angle) 보간
    // trackingSpeed가 클수록 Target에 빨리 도달. 
    // 기존 벡터 Lerp보다 반응이 느릴 수 있으므로 trackingSpeed를 2.0 -> 5.0~10.0 정도로 높이는 것 추천
    const _float t = std::clamp(m_sOptions.trackingSpeed * DELTA_TIME, 0.f, 1.f);

    m_fCurYaw = LerpAngle(m_fCurYaw, m_fTargetYaw, t);
    m_fCurPitch = LerpAngle(m_fCurPitch, m_fTargetPitch, t);

    // 2. 보간된 각도로 카메라 위치 계산 (구면 좌표계)
    const _float yawRad = Deg2Rad(m_fCurYaw);
    const _float pitchRad = Deg2Rad(m_fCurPitch);

    const _float cy = std::cos(yawRad);
    const _float sy = std::sin(yawRad);
    const _float cp = std::cos(pitchRad);
    const _float sp = std::sin(pitchRad);

    vector3 camForward;
    camForward.x = cp * sy;
    camForward.y = sp;
    camForward.z = cp * cy;

    // 3. 최종 위치 적용 (Vector Lerp 없이 직접 설정)
    // 각도가 부드럽게 변하므로 위치는 항상 Pivot을 중심으로 한 구면 위를 부드럽게 움직임
    const vector3 finalPos = pivot - camForward * m_fBackOffset;

    tf->Set_Position(finalPos);
    tf->LookAt(pivot);
}

void CPlayerCamera::OnDestroy()
{
    ClipCursor(nullptr);
}

const vector3 CPlayerCamera::Get_ForwardVector()
{
    // 현재 카메라가 바라보는 방향 (Y축 제외)
    vector3 forward = Get_Transform()->Get_Directions().forward;
    forward.y = 0.f;
    return forward.normalized();
}

const float CPlayerCamera::Get_ForwardAngle()
{
    vector3 f = Get_ForwardVector(); // 위에서 만든 함수 재활용

    const _float len2 = f.x * f.x + f.z * f.z;
    if (len2 < 1e-6f) return 0.f;

    // atan2로 각도 산출
    const _float yawRad = std::atan2(f.x, f.z);
    _float yawDeg = yawRad * (180.f / PI);

    return yawDeg;
}