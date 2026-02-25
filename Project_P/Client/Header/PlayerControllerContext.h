#pragma once

#include "cpch.h"
#include "PlayerController.h"
#include "PlayerControllerTypes.h"

class CPlayerControllerContext final : public UObject
{
private:
	typedef struct ContextValue_Move 
	{
		_float m_fMove01 = 0.f;
		vector3 m_vMoveLocalDir = {};
		vector3 m_vMoveWorldDir = {};
		_bool m_bBigTurnLatched = false;
		_float m_fBigTurnDeg = 120.f;
		_float m_fMoveLockTimer = 0.f;

		_float m_fDesiredYaw = 0.f;
		_bool  m_bTurning = false;
		_float m_targetYaw = 0.f;
		_float m_turnDir = 0.f;

		_bool m_bEvadeExit = 0.f;
		_float m_fEvadeExitTime = 0.f;
	}CV_MOVE;

	typedef struct ContextValue
	{
		_bool  m_bActive = false;
		_bool  m_bBuffered = false;
		_float m_fBufferT = 0.f;
		_float m_fBufferLife = 0.25f;
	}CONTEXT_VALUE;

public:
	CPlayerControllerContext();
	~CPlayerControllerContext();

public:
	void Bind(CPlayer* _player, CPlayerCamera* _cam, CPlayerController* _controller);

	const _bool IsRunning() const;

	const _bool IsKeyPressed_Hold(PlayerState state);
	const _bool IsKeyPressed_Down(PlayerState state);
	const _bool IsKeyPressed_UP(PlayerState state);

	vector3 CameraForward() const;
	_float CameraYawDeg() const;

	void StartMoveLock(_float _sec);

	const vector3& PlayerForward();

	void SetPlayerYaw(const _float _y);

	void AddPosition(const vector3& delta);

	void BeginTurnTo(_float _targetYawDeg);
	_float DeltaAngleDeg(float _current, _float _target);
	void TickMove();
	void TickTurn(_float _yawSmooth, _float stopEpsDeg = 1.f);

	CAnimator* Animator();
	CRigidBody* RigidBody();
	const CPlayer::PlayerStatus& PlayerStatus();

	void SetAnimMoveSpeed(_float _v);

	_bool IsTurning() const;
	void SetMoveLocalDir(const vector3& _dir);
	void SetMoveWorldDir(const vector3& _dir);
	void SetAnimTurn(const _float _value);
	const vector3& GetMoveLocalDir() const;
	const vector3& GetMoveWorldDir() const;
	const _float GetCameraYaw() const;
	_float WrapDeg(_float _deg) const;
	_float DeltaAngleDeg(_float _current, _float _target) const;
	void SetDesiredYawDeg(_float _yaw);
	_float GetDesiredYawDeg() const;

public:
	const CPlayerController::PlayerState CurrentState() const;
	void SetCurrentState(CPlayerController::PlayerState _state);
	const _bool IsBattle() const;
	void SetBattle(const _bool _value);

public:
	void BufferAction(PlayerState state);
	_bool ConsumeActionBuffer(PlayerState state);
	_bool HasActionBuffered(PlayerState state) const;
	void SetActionActive(PlayerState state, _bool v);
	_bool IsActionActive(PlayerState state) const;
	void TickActionBuffer(PlayerState state);

public:
	CPlayerController* Get_Controller();

	const _bool IsSprint() const;
	const _bool IsBigTurn() const;
	void SetSprint(const _bool _value);

	const _bool IsCanMove() const;
	void SetCanMove(const _bool _value);
	const _bool IsCanTurn() const;
	void SetCanTurn(const _bool _value);
	const _bool IsCanAttack() const;
	void SetCanAttack(const _bool _value);
	const _bool IsCanGuard() const;
	void SetCanGuard(const _bool _value);
	const _bool IsCanEvade() const;
	void SetCanEvade(const _bool _value);
	const _bool IsCanJump() const;
	void SetCanJump(const _bool _value);

	const _bool IsEvadeExit() const;
	void SetEvadeExit();

public:
	void StopMoveImmediate();

public:
	static _float LengthXZ(const vector3& v)
	{
		return sqrtf(v.x * v.x + v.z * v.z);
	}

	static vector3 NormalizeXZ(const vector3& v)
	{
		vector3 r = v;
		r.y = 0.f;
		float len = LengthXZ(r);
		if (len > 1e-6f)
		{
			r.x /= len; r.z /= len;
		}
		else
		{
			r.x = r.y = r.z = 0.f;
		}
		return r;
	}

private:
	CPlayer* m_pPlayer;
	CPlayerCamera* m_pCam;
	CPlayerController* m_pController;

private:
	_bool m_bSprint, m_bBigTurn;
	_bool m_bCanMove, m_bCanTurn, m_bCanAttack, m_bCanGuard, m_bCanEvade, m_bCanJump;

private:
	CV_MOVE m_Cv_Move;
	map<PlayerState, CONTEXT_VALUE> m_mBattleContext;

private:
	CPlayerController::PlayerState m_eCurrentState;
	CONTEXT_VALUE* GetBattleContext(PlayerState state);
	const CONTEXT_VALUE* GetBattleContext(PlayerState state) const;
};
