#include "cpch.h"
#include "PlayerControllerContext.h"
#include "PlayerController.h"

CPlayerControllerContext::CPlayerControllerContext()
	: m_pPlayer(nullptr)
	, m_pController(nullptr)
	, m_pCam(nullptr)
	, m_Cv_Move({})
	, m_Cv_Battle({})
	, m_bCanMove(true)
	, m_bCanTurn(true)
	, m_bCanAttack(true)
{
	m_strName = L"PlayerControllerContext";
}

CPlayerControllerContext::~CPlayerControllerContext()
{
}

void CPlayerControllerContext::Bind(CPlayer* _player, CPlayerCamera* _cam, CPlayerController* _controller)
{
	m_pPlayer = _player;
	m_pCam = _cam;
	m_pController = _controller;
}

void CPlayerControllerContext::SetMovePressed(_bool pressed)
{
	m_Cv_Move.m_bMovePressed = pressed;
}

const _bool CPlayerControllerContext::IsMovePressed() const
{
	return m_Cv_Move.m_bMovePressed;
}

const _bool CPlayerControllerContext::IsLightAttackPressed()
{
	return m_pController->m_mKeyDown[CPlayerController::Attack];
}

const _bool CPlayerControllerContext::IsGuardPressed()
{
	return 	m_pController->m_mKeyHold[CPlayerController::Guard];
}

const _bool CPlayerControllerContext::IsGuardPressed_Down()
{
	return 	m_pController->m_mKeyDown[CPlayerController::Guard];
}

vector3 CPlayerControllerContext::CameraForward() const
{
	return m_pCam ? m_pCam->Get_ForwardVector() : vector3::forward();
}

_float CPlayerControllerContext::CameraYawDeg() const
{
	return m_pCam ? m_pCam->Get_ForwardAngle() : 0.f;
}

void CPlayerControllerContext::StartMoveLock(_float _sec)
{
	m_Cv_Move.m_fMoveLockTimer = max(m_Cv_Move.m_fMoveLockTimer, _sec);
}

const vector3& CPlayerControllerContext::PlayerForward()
{
	return m_pPlayer->Get_Transform()->Get_Directions().forward;
}

void CPlayerControllerContext::SetPlayerYaw(const _float _y)
{
	m_pPlayer->Get_Transform()->Set_LocalEulerAngles(0.f, _y, 0.f);
}

void CPlayerControllerContext::AddPosition(const vector3& delta)
{
	if (!m_pPlayer) 
		return;
	m_pPlayer->Get_Transform()->Add_Position(delta);
}

void CPlayerControllerContext::BeginTurnTo(_float _targetYawDeg)
{
	m_Cv_Move.m_targetYaw = WrapDeg(_targetYawDeg);
	if (!m_Cv_Move.m_bTurning && m_pPlayer)
	{
		const _float curYaw = m_pPlayer->Get_Transform()->Get_EulerAngles().y;
		const _float delta = DeltaAngleDeg(curYaw, m_Cv_Move.m_targetYaw);
		if (fabsf(delta) >= 1e-4f)
			m_Cv_Move.m_turnDir = (delta > 0.f) ? 1.f : -1.f;
		else
			m_Cv_Move.m_turnDir = 0.f;
	}

	m_Cv_Move.m_bTurning = true;
}

_float CPlayerControllerContext::DeltaAngleDeg(float _current, _float _target)
{
	float delta = fmodf(_target - _current, 360.f);
	if (delta > 180.f) 
		delta -= 360.f;
	if (delta < -180.f)
		delta += 360.f;
	return delta;
}

static _float MoveTowards1D(_float cur, _float target, _float maxDelta)
{
	if (cur < target)
		return (cur + maxDelta > target) ? target : (cur + maxDelta);
	if (cur > target)
		return (cur - maxDelta < target) ? target : (cur - maxDelta);
	return target;
}

void CPlayerControllerContext::TickMove()
{
	if (!m_pPlayer) 
		return;

	if (!m_bCanMove)
		return;

	_float dt = DELTA_TIME;
            
	dt = std::clamp(dt, 0.f, 0.05f);

	if (m_Cv_Move.m_fMoveLockTimer > 0.f)
	{
		m_Cv_Move.m_fMoveLockTimer -= dt;

		m_Cv_Move.m_fMove01 = 0.f;
		return;
	}

	const _float target01 = m_Cv_Move.m_bMovePressed ? 1.f : 0.f;
	const _float rate = (target01 > m_Cv_Move.m_fMove01) ? PlayerStatus().moveAccelRate : PlayerStatus().moveDecelRat;
	const _float maxDelta = rate * dt;

	m_Cv_Move.m_fMove01 = MoveTowards1D(m_Cv_Move.m_fMove01, target01, maxDelta);

	vector3 dir = m_Cv_Move.m_vMoveWorldDir;
	dir.y = 0.f;

	const _float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
	if (len > 1e-6f) 
	{ 
		dir.x /= len; dir.z /= len; 
	}
	else 
	{
		dir = vector3(0.f, 0.f, 0.f); 
	}

	const _float curSpeed = PlayerStatus().moveSpeed * m_Cv_Move.m_fMove01;
	AddPosition(dir * curSpeed * dt);
	
	SetAnimMoveSpeed(m_Cv_Move.m_fMove01);
}

void CPlayerControllerContext::TickTurn(_float _yawSmooth, _float stopEpsDeg)
{
	if (!m_pPlayer) 
		return;

	if (!m_bCanTurn)
		return;

	if (!m_Cv_Move.m_bTurning)
	{
		SetAnimTurn(0.f);
		m_Cv_Move.m_bBigTurnLatched = false;
		m_Cv_Move.m_turnDir = 0.f;
		return;
	}

	auto tr = m_pPlayer->Get_Transform();
	vector3 e = tr->Get_EulerAngles();
	_float curYaw = e.y;

	_float delta = DeltaAngleDeg(curYaw, m_Cv_Move.m_targetYaw);
	_float absDelta = fabsf(delta);

	if (m_Cv_Move.m_turnDir == 0.f && absDelta >= stopEpsDeg)
		m_Cv_Move.m_turnDir = (delta > 0.f) ? 1.f : -1.f;

	m_pPlayer->Get_Animator()->ResetTrigger(L"turn");

	if (!m_Cv_Move.m_bBigTurnLatched && absDelta >= m_Cv_Move.m_fBigTurnDeg && m_Cv_Move.m_bMovePressed)
	{  
		SetAnimTurn(m_Cv_Move.m_turnDir);
		m_pPlayer->Get_Animator()->SetTrigger(L"turn");

		StartMoveLock(PlayerStatus().bigTurnStopSec);

		m_Cv_Move.m_bBigTurnLatched = true;
	}

	_float t = 1.f - expf(-_yawSmooth * DELTA_TIME);
	t = std::clamp(t, 0.f, 1.f);

	_float newYaw = WrapDeg(curYaw + delta * t);
	tr->Set_EulerAngles(e.x, newYaw, e.z);

	if (fabsf(DeltaAngleDeg(tr->Get_EulerAngles().y, m_Cv_Move.m_targetYaw)) < stopEpsDeg)
	{
		tr->Set_EulerAngles(e.x, m_Cv_Move.m_targetYaw, e.z);
		m_Cv_Move.m_bTurning = false;
		SetAnimTurn(0.f);

		m_Cv_Move.m_bBigTurnLatched = false;
		m_Cv_Move.m_turnDir = 0.f;
	}
}

CAnimator* CPlayerControllerContext::Animator()
{
	return m_pPlayer->Get_Animator();
}

const CPlayer::PlayerStatus& CPlayerControllerContext::PlayerStatus()
{
	return m_pPlayer->Get_PlayerStatus();
}

void CPlayerControllerContext::SetAnimMoveSpeed(_float _v)
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
	return m_Cv_Move.m_bTurning;
}

void CPlayerControllerContext::SetMoveWorldDir(const vector3& _dir)
{
	m_Cv_Move.m_vMoveWorldDir = _dir;
}

void CPlayerControllerContext::SetAnimTurn(const _float _value)
{
	if (!m_pPlayer)
		return;
	auto anim = m_pPlayer->Get_Animator();
	if (!anim)
		return;

	anim->SetFloat(L"turnDir", _value);
}

const vector3& CPlayerControllerContext::GetMoveWorldDir() const
{
	return m_Cv_Move.m_vMoveWorldDir;
}

const _float CPlayerControllerContext::GetCameraYaw() const
{
	return m_pCam->Get_ForwardAngle();
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
	m_Cv_Move.m_fDesiredYaw = _yaw;
}

_float CPlayerControllerContext::GetDesiredYawDeg() const
{
	return m_Cv_Move.m_fDesiredYaw;
}

const _bool CPlayerControllerContext::IsBattle() const
{
	return m_pController->IsBattle();
}

void CPlayerControllerContext::SetBattle(const _bool _value)
{
	m_pController->SetBattle(_value);
	m_pPlayer->Get_Animator()->SetTrigger(L"BattleEnd");
}

void CPlayerControllerContext::BufferAttack()
{
	m_Cv_Battle.m_bAttackBuffered = true;
	m_Cv_Battle.m_fAttackBufferT = 0.f;
}

_bool CPlayerControllerContext::ConsumeAttackBuffer()
{
	if (!m_Cv_Battle.m_bAttackBuffered)
		return false;
	m_Cv_Battle.m_bAttackBuffered = false;
	m_Cv_Battle.m_fAttackBufferT = 0.f;
	return true;
}

_bool CPlayerControllerContext::HasAttackBuffered() const
{
	return m_Cv_Battle.m_bAttackBuffered;
}

void CPlayerControllerContext::SetAttackActive(_bool v)
{
	m_Cv_Battle.m_bAttackActive = v;
}

_bool CPlayerControllerContext::IsAttackActive() const
{
	return m_Cv_Battle.m_bAttackActive;
}

void CPlayerControllerContext::BufferGuard()
{
	m_Cv_Battle.m_bGuardBuffered = true;
	m_Cv_Battle.m_fGuardBufferT = 0.f;
}

_bool CPlayerControllerContext::ConsumeGuardBuffer()
{
	if (!m_Cv_Battle.m_bGuardBuffered)
		return false;
	m_Cv_Battle.m_bGuardBuffered = false;
	m_Cv_Battle.m_fGuardBufferT = 0.f;
	return true;
}

_bool CPlayerControllerContext::HasGuardBuffered() const
{
	return m_Cv_Battle.m_bGuardBuffered;
}

void CPlayerControllerContext::SetGuardActive(_bool v)
{
	m_Cv_Battle.m_bGuardActive = v;
}

_bool CPlayerControllerContext::IsGuardActive() const
{
	return 	m_Cv_Battle.m_bGuardActive;
}

void CPlayerControllerContext::TickAttackBuffer()
{
	if (!m_Cv_Battle.m_bAttackBuffered)
		return;

	_float dt = DELTA_TIME;
     
	dt = std::clamp(dt, 0.f, 0.05f);

	m_Cv_Battle.m_fAttackBufferT += dt;

	if (m_Cv_Battle.m_fAttackBufferT >= m_Cv_Battle.m_fAttackBufferLife)
	{
		m_Cv_Battle.m_bAttackBuffered = false;
		m_Cv_Battle.m_fAttackBufferT = 0.f;
	}
}

void CPlayerControllerContext::TickGuardBuffer()
{
	if (!m_Cv_Battle.m_bGuardBuffered)
		return;

	_float dt = DELTA_TIME;

	dt = std::clamp(dt, 0.f, 0.05f);

	m_Cv_Battle.m_fGuardBufferT += dt;

	if (m_Cv_Battle.m_fGuardBufferT >= m_Cv_Battle.m_fGuardBufferLife)
	{
		m_Cv_Battle.m_bGuardBuffered = false;
		m_Cv_Battle.m_fGuardBufferT = 0.f;
	}
}

const _bool CPlayerControllerContext::IsCanMove() const
{
	return m_bCanMove;
}

void CPlayerControllerContext::SetCanMove(const _bool _value)
{
	m_bCanMove = _value;
}

const _bool CPlayerControllerContext::IsCanTurn() const
{
	return m_bCanTurn;
}

void CPlayerControllerContext::SetCanTurn(const _bool _value)
{
	m_bCanTurn = _value;
}

const _bool CPlayerControllerContext::IsCanAttack() const
{
	return m_bCanAttack;
}

void CPlayerControllerContext::SetCanAttack(const _bool _value)
{
	m_bCanAttack = _value;
}

void CPlayerControllerContext::StopMoveImmediate()
{
	m_Cv_Move.m_bMovePressed = false;
	m_Cv_Move.m_fMove01 = 0.f;
	m_Cv_Move.m_vMoveWorldDir = vector3::zero();
	m_Cv_Move.m_fMoveLockTimer = 0.f;

	SetAnimMoveSpeed(0.f);
}
