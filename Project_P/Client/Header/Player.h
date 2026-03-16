#pragma once

#include "Character.h"

class CPlayer : public CCharacter
{
	friend class CGameObject;

public:
	enum PlayerAnimationStatus { Idle, Run, CombatIdle };

public:
	struct StaticPlayerStatus
	{
		const static _uint HPMAX = 12;
	};

	struct PlayerStatus
	{
		_int maxBetaEnergy = 0;
		_int crtBeatEnergy = 0;
		_int maxHp = 300;
		_int crtHp = 0;
		_int maxShield;
		_int crtShield = 0;
		_int attackPower = 0;
		_int shAttack = 0;
		_float criticalChance = 0.f;
		_float criticalDamage = 0.f;
		_float runSpeed = 3.f;
		_float sprintSpeed = 4.2f;
		_float backWalkRatio = 0.7f;
		_float moveAccelRate = 4.f;
		_float moveDecelRat = 1.5f;
		_float turnSpeed = 8.f;
		_float bigTurnStopSec = 0.28f;
		_float focusTurnRatio = 8.f;
		_float evadeLength = 5.f;
		_float jumpPower = 340.f;
		_float hitKnockbackSpeed = 3.f;
	};

	struct EquipStatus
	{
		_int be = 1000;
		_int hp = 2000;
		_int sh = 500;
		_int attack = 450;
		_int shAttack = 100;
		_float criticalChance = 5.f;
		_float criticalDamageRate = 150.f;
	};

protected:
	CPlayer();
	~CPlayer();

protected:
	static CPlayer* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void Start() override;
	void Update() override;
	void FixedUpdate() override;
	void OnDestroy() override;

public:
	class CPlayerController* Get_Controller();

public:
	const PlayerStatus& Get_PlayerStatus();
	const PlayerStatus Get_PlayerEquipStat();
	void RecoverHp(const _uint _value);
	void GetDamage(const _uint _damage);

	const _float GetRadius() const;

public:
	const _uint GetLightAttackComboCount() const;
	void SetLightAttakComboCount(const _uint _count);

public:
	void OnSwordAttackHandler();
	void DisableSwordCollider();
	void GetHitHandler(const HurtDescription& _hurtDesc) override;

protected:
	void SetAnimationAction() override;

private:
	_bool CanGuardHit(const HurtDescription& _hurtDesc);
	_bool TryGuardHit(const HurtDescription& _hurtDesc);
	void SyncHUDStatus();

private:
	class CPlayerController* m_pController;
	CGameObject* m_pHeadObj, *m_pHairObj, *m_pPonyTailObj;

	class CWeapon* m_pEquipWeapon;

	PlayerStatus m_sPlayerStatus;
	EquipStatus m_sEquipStatus;

	vector<CSkinnedMeshRenderer*> m_vBodySuits, m_vFaces, m_vHairs, m_vPonyTailas;

	CTransform* m_pWeaponHolder;

public:
	_uint m_iLightAttackComboCount;
};


