#pragma once

#include "epch.h"

class CPlayer : public CComponent
{
public:
	enum PlayerAnimationStatus { Idle, Run, CombatIdle };

public:
	struct StaticPlayerStatus
	{
		const static _uint HPMAX = 12;
	};

	struct PlayerStatus
	{
		_int maxHp = 6;
		_int crtHp = 0;
		_float moveSpeed = 4.f;
		_float backWalkRatio = 0.7f;
		_float turnSpeed = 180.f;
		_float focusTurnRatio = 8.f;
		_int attackPower = 1;
	};

protected:
	CPlayer();
	~CPlayer();

public:
	static CPlayer* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;

public:
	void Set_Focus(CTransform* _transform);
	void RecoverHp(const _uint _value);
	void GetDamage(const _uint _damage);

private:
	void PlayerControle();
	void PlayerControle_NoneLockOn();
	void PlayerControle_LockOn();
	void PlayerControle_AttackCombo();

	void PlayIdleAnimation(const _float _blending = 0.1f);
	void PlayMoveAnimation(const _float _blending = 0.1f);
	void PlayJumpAnimation(const _float _blending = 0.1f);
	
	void PlaySwordAnimation();

private:
	CSkinnedMeshRenderer* m_pSkinnedMeshRenderer;
	CAnimator* m_pAnimator;

	class CWeapon* m_pEquipWeapon;

	PlayerStatus m_sPlayerStatus;
	PlayerAnimationStatus m_eAnimationStatus;

	_float m_fCrtMoveSpeed;

	vector3 m_vMoveDirection, m_vPrevMoveDirectoin;
	_float m_fRotateDirection, m_fPrevRotateDirection;

	_bool m_bNotMoveTurning, m_bBackMove;
	_bool m_bLockOnMode, m_bIsCombatMode;
	_bool m_bPrevLockOnMode;
	_bool m_bIsAttack, m_bIsPrevAttack;
	_bool m_bSwordActionDuring;

	_float m_fSwordActionEndFrames[3];

	_float m_fAttackComboNT;
	_uint m_iAttackComboDest;

	_bool m_bIsJump, m_bIsPrevJump;

	CTransform* m_pFocusTransform;
};

