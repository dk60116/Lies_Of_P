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
		_float runSpeed = 3.f;
		_float sprintSpeed = 4.2f;
		_float backWalkRatio = 0.7f;
		_float moveAccelRate = 4.f;
		_float moveDecelRat = 1.5f;
		_float turnSpeed = 8.f;
		_float bigTurnStopSec = 0.28f;
		_float focusTurnRatio = 8.f;
		_int attackPower = 1;
		_float jumpPower = 300.f;
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
	class CPlayerController* Get_Controller();
	CAnimator* Get_Animator();
	CRigidBody* Get_RigidBody();

public:
	const PlayerStatus& Get_PlayerStatus();
	void RecoverHp(const _uint _value);
	void GetDamage(const _uint _damage);

private:
	class CPlayerController* m_pController;
	CGameObject* m_pHeadObj, *m_pHairObj;

	CSkinnedMeshRenderer* m_pSkinnedMeshRenderer;
	CAnimator* m_pAnimator;

	class CWeapon* m_pEquipWeapon;

	PlayerStatus m_sPlayerStatus;

	vector<CSkinnedMeshRenderer*> m_vBodySuits, m_vFaces, m_vHairs;

	CTransform* m_pWeaponHolder;

	CCapsuleCollider* m_pBodyCollider;
	CRigidBody* m_pRigidBody;

public:
	const _uint GetLightAttackComboCount() const;
	void SetLightAttakComboCount(const _uint _count);

public:
	_uint m_iLightAttackComboCount;
};

