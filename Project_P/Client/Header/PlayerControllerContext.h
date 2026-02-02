#pragma once

#include "cpch.h"

class CPlayerControllerContext final : public UObject
{
public:
	CPlayerControllerContext();
	~CPlayerControllerContext();

public:
	void Bind(CPlayer* _player, CPlayerCamera* _cam);

	void SetMovePressed(_bool pressed);
	_bool IsMovePressed() const;

	vector3 CameraForward() const;
	_float CameraYawDeg() const;

	void AddPosition(const vector3& delta);

	void BeginTurnTo(_float _targetYawDeg);
	_float DeltaAngleDeg(float _current, _float _target);
	void TickTurn(_float _yawSmooth = 12.f, _float stopEpsDeg = 1.f);

	const CPlayer::PlayerStatus& PlayerStat();

	void SetAnimSpeed(_float _v);

	bool IsTurning() const;

public:
	void SetMoveWorldDir(const vector3& _dir);
	void SetAnimTurn(const _float _value);
	const vector3& GetMoveWorldDir() const;
	_float WrapDeg(_float _deg) const;
	_float DeltaAngleDeg(_float _current, _float _target) const;
	void SetDesiredYawDeg(_float _yaw);
	_float GetDesiredYawDeg() const;

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

	_bool  m_bMovePressed;

	vector3 m_vMoveWorldDir;
	_float m_fDesiredYaw;

	_bool  m_bTurning;
	_float m_targetYaw;
};

