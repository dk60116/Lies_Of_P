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
		_float moveSpeed = 6.f;
		_float backWalkRatio = 0.7f;
		_float turnSpeed = 280.f;
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
	class CPlayerController* Get_Controller();

public:
	void Set_Focus(CTransform* _transform);
	void RecoverHp(const _uint _value);
	void GetDamage(const _uint _damage);

private:
	class CPlayerController* m_pController;
	CGameObject* m_pHeadObj, *m_pHairObj;

	CSkinnedMeshRenderer* m_pSkinnedMeshRenderer;
	CAnimator* m_pAnimator;

	class CWeapon* m_pEquipWeapon;

	PlayerStatus m_sPlayerStatus;
	PlayerAnimationStatus m_eAnimationStatus;

	_float m_fSwordActionEndFrames[3];

	_float m_fAttackComboNT;
	_uint m_iAttackComboDest;

	_bool m_bIsJump, m_bIsPrevJump;

	CTransform* m_pFocusTransform;

	vector<CSkinnedMeshRenderer*> m_vBodySuits, m_vFaces, m_vHairs;
};

