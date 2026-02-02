#pragma once

#include "cpch.h"

class CPlayerControllerContext final : public UObject
{
private:
	typedef struct ContextValue_Move 
	{
		_bool  m_bMovePressed = false;
		_float m_fMove01 = 0.f;
		vector3 m_vMoveWorldDir = {};
		_bool m_bBigTurnLatched = false;
		_float m_fBigTurnDeg = 120.f;
		_float m_fMoveLockTimer = 0.f;

		_float m_fDesiredYaw = 0.f;
		_bool  m_bTurning = false;
		_float m_targetYaw = 0.f;
		_float m_turnDir = 0.f;
	}CV_MOVE;

	typedef struct ContextValue_Battle
	{
		_bool  m_bAttackActive = false;
		_bool  m_bAttackBuffered = false;
		_float m_fAttackBufferT = 0.f;
		_float m_fAttackBufferLife = 0.25f;
	}CV_BATTLE;

public:
	CPlayerControllerContext();
	~CPlayerControllerContext();

public:
	void Bind(CPlayer* _player, CPlayerCamera* _cam);

	void SetMovePressed(_bool pressed);
	_bool IsMovePressed() const;

	vector3 CameraForward() const;
	_float CameraYawDeg() const;

	void StartMoveLock(_float _sec);

	void AddPosition(const vector3& delta);

	void BeginTurnTo(_float _targetYawDeg);
	_float DeltaAngleDeg(float _current, _float _target);
	void TickMove();
	void TickTurn(_float _yawSmooth, _float stopEpsDeg = 1.f);

	CAnimator* Animator();
	const CPlayer::PlayerStatus& PlayerStat();

	void SetAnimSpeed(_float _v);

	_bool IsTurning() const;
	void SetMoveWorldDir(const vector3& _dir);
	void SetAnimTurn(const _float _value);
	const vector3& GetMoveWorldDir() const;
	_float WrapDeg(_float _deg) const;
	_float DeltaAngleDeg(_float _current, _float _target) const;
	void SetDesiredYawDeg(_float _yaw);
	_float GetDesiredYawDeg() const;

public:
	void BufferAttack();
	_bool ConsumeAttackBuffer();
	_bool HasAttackBuffered() const;

	void SetAttackActive(_bool v);
	_bool IsAttackActive() const;

	void TickAttackBuffer();

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

private:
	CV_MOVE m_Cv_Move;
	CV_BATTLE m_Cv_Battle;
};
