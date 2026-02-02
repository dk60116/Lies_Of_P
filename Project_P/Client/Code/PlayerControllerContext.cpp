#include "cpch.h"
#include "PlayerControllerContext.h"

CPlayerControllerContext::CPlayerControllerContext()
	: m_pPlayer(nullptr)
	, m_pCam(nullptr)
	, m_bMovePressed(false)
	, m_vMoveWorldDir({})
	, m_fDesiredYaw(0.f)
	, m_bTurning(false)
	, m_targetYaw(0.f)
{
	m_strName = L"PlayerControllerContext";
}

CPlayerControllerContext::~CPlayerControllerContext()
{
}

void CPlayerControllerContext::Bind(CPlayer* _player, CPlayerCamera* _cam)
{
	m_pPlayer = _player;
	m_pCam = _cam;
}

void CPlayerControllerContext::SetMovePressed(_bool pressed)
{
	m_bMovePressed = pressed;
}

_bool CPlayerControllerContext::IsMovePressed() const
{
	return m_bMovePressed;
}

vector3 CPlayerControllerContext::CameraForward() const
{
	return m_pCam ? m_pCam->Get_ForwardVector() : vector3::forward();
}

_float CPlayerControllerContext::CameraYawDeg() const
{
	return m_pCam ? m_pCam->Get_ForwardAngle() : 0.f;
}

void CPlayerControllerContext::AddPosition(const vector3& delta)
{
	if (!m_pPlayer) 
		return;
	m_pPlayer->Get_Transform()->Add_Position(delta);
}

void CPlayerControllerContext::BeginTurnTo(_float _targetYawDeg)
{
	m_targetYaw = WrapDeg(_targetYawDeg);
	m_bTurning = true;
}

_float CPlayerControllerContext::DeltaAngleDeg(float _current, _float _target)
{
	float delta = fmodf(_target - _current, 360.f);
	if (delta > 180.f)  delta -= 360.f;
	if (delta < -180.f) delta += 360.f;
	return delta;
}

void CPlayerControllerContext::TickTurn(_float _yawSmooth, _float stopEpsDeg)
{
	if (!m_bTurning || !m_pPlayer) return;

	auto tr = m_pPlayer->Get_Transform();
	vector3 e = tr->Get_EulerAngles();
	_float curYaw = e.y;

	_float t = 1.f - expf(-_yawSmooth * DELTA_TIME);

	_float newYaw = WrapDeg(curYaw + DeltaAngleDeg(curYaw, m_targetYaw) * t);
	tr->Set_EulerAngles(e.x, newYaw, e.z);

	if (fabsf(DeltaAngleDeg(tr->Get_EulerAngles().y, m_targetYaw)) < stopEpsDeg)
	{
		tr->Set_EulerAngles(e.x, m_targetYaw, e.z);
		m_bTurning = false;
	}
}

const CPlayer::PlayerStatus& CPlayerControllerContext::PlayerStat()
{
	return m_pPlayer->Get_PlayerStatus();
}

void CPlayerControllerContext::SetAnimSpeed(_float _v)
{
	if (!m_pPlayer)
		return;
	auto anim = m_pPlayer->Get_Animator();
	if (!anim)
		return;

	anim->SetFloat(L"speed", _v);
}

bool CPlayerControllerContext::IsTurning() const
{
	return m_bTurning;
}

void CPlayerControllerContext::SetMoveWorldDir(const vector3& _dir)
{
	m_vMoveWorldDir = _dir;
}

const vector3& CPlayerControllerContext::GetMoveWorldDir() const
{
	return m_vMoveWorldDir;
}

_float CPlayerControllerContext::WrapDeg(_float _deg) const
{
	while (_deg >= 360.f) 
		_deg -= 360.f; 
	while (_deg < 0.f) 
		_deg += 360.f; 
	return _deg;
}

_float CPlayerControllerContext::DeltaAngleDeg(_float _current, _float _target) const
{
	_float delta = fmodf(_target - _current, 360.f);
	if (delta > 180.f)
		delta -= 360.f;
	if (delta < -180.f) 
		delta += 360.f;
	return delta;
}

void CPlayerControllerContext::SetDesiredYawDeg(_float _yaw)
{
	m_fDesiredYaw = _yaw;
}

_float CPlayerControllerContext::GetDesiredYawDeg() const
{
	return m_fDesiredYaw;
}
