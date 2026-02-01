#include "cpch.h"
#include "PlayerController.h"
#include "Player.h"

CPlayerController::CPlayerController()
	: m_pPlayer(nullptr)
	, m_pPlayerCam(nullptr)
	, m_bRunning(nullptr)
	, m_bTurning(false)
	, m_mKeyHold({})
	, m_vMoveDirection({})
{
}

CPlayerController::~CPlayerController()
{
}

CPlayerController* CPlayerController::Create()
{
	return new CPlayerController();
}

CComponent* CPlayerController::Clone() const
{
	CPlayerController* clone = new CPlayerController();
	return clone;
}

HRESULT CPlayerController::Initialize()
{
	m_mKeyHold.insert({ Forward, false });
	m_mKeyHold.insert({ Back, false });
	m_mKeyHold.insert({ Left, false });
	m_mKeyHold.insert({ Right, false });

	return S_OK;
}

void CPlayerController::Awake()
{
	m_pPlayerCam = CGameManager::GetInstance().Get_PlayerCamera();
}

void CPlayerController::Start()
{
}

void CPlayerController::Update()
{
	Update_Key();
	Update_Move();
}

void CPlayerController::LateUpdate()
{
}

void CPlayerController::OnDestroy()
{
}

void CPlayerController::Set_Player(CPlayer* _player)
{
	m_pPlayer = _player;
}

void CPlayerController::Set_Camera(CPlayerCamera* _cam)
{
	m_pPlayerCam = _cam;
}

const _bool CPlayerController::IsRunning() const
{
	return m_bRunning;
}

void CPlayerController::Update_Key()
{
	m_mKeyHold[Forward] = CInput::GetInstance().GetKey(W);
	m_mKeyHold[Back] = CInput::GetInstance().GetKey(S);
	m_mKeyHold[Left] = CInput::GetInstance().GetKey(A);
	m_mKeyHold[Right] = CInput::GetInstance().GetKey(D);
}

const _float CPlayerController::WrapDeg(_float deg) const
{
	while (deg >= 360.f)
		deg -= 360.f;
	while (deg < 0.f)
		deg += 360.f;
	return deg;
}

const _float CPlayerController::DeltaAngleDeg(_float current, _float target) const
{
	_float delta = fmodf(target - current, 360.f);
	if (delta > 180.f)
		delta -= 360.f;
	if (delta < -180.f)
		delta += 360.f;
	return delta;
}

void CPlayerController::Update_Move()
{
	m_bRunning = m_mKeyHold[Forward] || m_mKeyHold[Back] || m_mKeyHold[Left] || m_mKeyHold[Right];

	vector3 camForward = m_pPlayerCam->Get_ForwardVector();
	float targetYaw = m_pPlayerCam->Get_ForwardAngle();

	auto tr = m_pPlayer->Get_Transform();

	if (m_bRunning)
	{
		m_pPlayer->Get_Transform()->Add_Position(camForward * 6.f * DELTA_TIME);

		m_bTurning = true;
	}

	if (m_bTurning)
	{
		vector3 e = tr->Get_EulerAngles();
		_float curYaw = e.y;

		const _float yawSmooth = 12.f;
		_float t = 1.f - expf(-yawSmooth * DELTA_TIME);

		_float newYaw = WrapDeg(curYaw + DeltaAngleDeg(curYaw, targetYaw) * t);

		tr->Set_EulerAngles(e.x, newYaw, e.z);

		if (abs(tr->Get_EulerAngles().y - targetYaw) < 1.f)
			m_bTurning = false;
	}
}