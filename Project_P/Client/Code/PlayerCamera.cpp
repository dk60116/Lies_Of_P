#include "cpch.h"
#include "PlayerCamera.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr _float PI = 3.14159265358979323846f;
    inline _float Deg2Rad(_float deg) { return deg * (PI / 180.f); }
    inline _float WrapDeg(_float deg)
    {
        while (deg >= 360.f) deg -= 360.f;
        while (deg < 0.f)   deg += 360.f;
        return deg;
    }
}

CPlayerCamera::CPlayerCamera()
    : m_pPlayer(nullptr)
    , m_sOptions({})
    , m_fBackOffset(6.f)
    , m_fZoomSensor(0.f)
{
}

CPlayerCamera::~CPlayerCamera()
{
    m_strName = L"PlayerCamera";
}

CPlayerCamera* CPlayerCamera::Create()
{
    return new CPlayerCamera();
}

CComponent* CPlayerCamera::Clone() const
{
    CPlayerCamera* clone = new CPlayerCamera();
    return clone;
}

HRESULT CPlayerCamera::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    m_pGameObject->AddComponent<CCamera>();

    // 줌 속도(기존 firstZoomSensor를 zoomSpeed로 쓰거나 그대로 써도 됨)
    m_fZoomSensor = m_sOptions.firstZoomSensor;

    // 초기 각도(원하면 옵션으로 빼세요)
    m_fYawDeg = 0.f;
    m_fPitchDeg = 15.f;

    return S_OK;
}

void CPlayerCamera::Awake()
{
    m_pPlayer = CGameManager::GetInstance().Get_Player();
}

void CPlayerCamera::Start()
{
}

namespace
{
    inline POINT GetClientCenterScreen(HWND hWnd)
    {
        RECT rc{};
        GetClientRect(hWnd, &rc);

        POINT c{};
        c.x = (rc.left + rc.right) / 2;
        c.y = (rc.top + rc.bottom) / 2;

        ClientToScreen(hWnd, &c);
        return c;
    }

    inline RECT GetClientRectScreen(HWND hWnd)
    {
        RECT rc{};
        GetClientRect(hWnd, &rc);

        POINT lt{ rc.left, rc.top };
        POINT rb{ rc.right, rc.bottom };
        ClientToScreen(hWnd, &lt);
        ClientToScreen(hWnd, &rb);

        RECT out{ lt.x, lt.y, rb.x, rb.y };
        return out;
    }
}

void CPlayerCamera::Update()
{
    HWND hWnd = CDisplay::GetInstance().Get_GameWindow(); 

    if (!hWnd || GetForegroundWindow() != hWnd)
    {
        ClipCursor(nullptr);
        m_bIgnoreNextDelta = true;
        return;
    }

    //// 1) 커서를 창 내부로 제한(선택사항이지만 추천)
    //if (m_bMouseLocked)
    //{
    //    RECT clip = GetClientRectScreen(hWnd);
    //    ClipCursor(&clip);
    //}
    //else
    //{
    //    ClipCursor(nullptr);
    //}

    // 2) 중앙 기준 델타 계산 + 중앙으로 워프
    _float dx = 0.f, dy = 0.f;
    if (m_bMouseLocked)
    {
        POINT center = GetClientCenterScreen(hWnd);

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

        // 3) 회전 적용 (픽셀 델타는 dt를 곱하지 않는 게 보통 가장 안정적)
        const _float sens = (m_sOptions.lookSensitivity > 0.f) ? m_sOptions.lookSensitivity : 0.15f;

        m_fYawDeg += dx * sens;
        m_fPitchDeg -= dy * sens;

        m_fYawDeg = WrapDeg(m_fYawDeg);

        const _float pitchMin = (m_sOptions.pitchMin != 0.f) ? m_sOptions.pitchMin : -35.f;
        const _float pitchMax = (m_sOptions.pitchMax != 0.f) ? m_sOptions.pitchMax : 70.f;
        m_fPitchDeg = std::clamp(m_fPitchDeg, pitchMin, pitchMax);
    }

    // 4) 줌(휠) - 휠은 보통 dt 곱 안 해도 됩니다만, 현재 구조 유지해도 OK
    _float wheel = CInput::GetInstance().GetAxis(L"Mouse ScrollWheel");
    if (wheel != 0.f)
    {
        m_fBackOffset -= wheel * m_fZoomSensor * DELTA_TIME;
        m_fBackOffset = std::clamp(m_fBackOffset, m_sOptions.zoomMin, m_sOptions.zoomMax);
    }
}

void CPlayerCamera::LateUpdate()
{
    if (!m_pPlayer)
        return;

    CTransform* tf = Get_Transform();
    CTransform* playerTf = m_pPlayer->Get_Transform();

    const vector3 playerPos = playerTf->Get_Position();

    // 피벗(카메라가 바라보고 공전하는 중심점)
    // 기존 lookHeightOffset을 그대로 “피벗 높이”로 쓰는 게 자연스럽습니다.
    const vector3 pivot = playerPos + vector3::up() * m_sOptions.lookHeightOffset;

    // yaw/pitch -> 방향 벡터(카메라가 pivot을 향해 보는 forward)
    const _float yawRad = Deg2Rad(m_fYawDeg);
    const _float pitchRad = Deg2Rad(m_fPitchDeg);

    const _float cy = std::cos(yawRad);
    const _float sy = std::sin(yawRad);
    const _float cp = std::cos(pitchRad);
    const _float sp = std::sin(pitchRad);

    // (x,z) 평면에서 yaw, y축으로 pitch
    // 기본 전제: +Z가 forward인 좌표계(지금 코드 스타일상 대체로 이 케이스)
    vector3 camForward;
    camForward.x = cp * sy;
    camForward.y = sp;
    camForward.z = cp * cy;

    // 목표 카메라 위치: pivot 뒤로 distance만큼
    const vector3 desiredPos = pivot - camForward * m_fBackOffset;

    // 추적 스무딩
    const _float followT = std::clamp(m_sOptions.trackingSpeed * DELTA_TIME, 0.f, 1.f);
    tf->Set_Position(vector3::Lerp(tf->Get_Position(), desiredPos, followT));

    // 바라보는 지점(소울라이크는 보통 pivot을 본다)
    tf->LookAt(pivot);
}

void CPlayerCamera::OnDestroy()
{
}
