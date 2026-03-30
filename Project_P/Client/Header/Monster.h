#pragma once

#include "Character.h"
#include "MonsterHUD.h"

class CMonster abstract : public CCharacter
{
	friend class CMonsterController;

public:
	struct MonsterStatus
	{
		_int maxHp = 1000;
		_int crtHp = 0;
		_int maxShield = 1000;
		_int crtShield = 0;
		_int maxBalance = 3;
		_int crtBalance = 0;
		_float moveSpeed = 5.f;
		_float detectionRange = 30.f;
		_float turnSpeed = 180.f;
		_int attackPower = 100;
		_float attackRange = 1.f;
		_float attackSpeed = 0.2f;
		_float hitKnockbackSpeed = 8.f;
		_float hitKnockbackDuration = 0.2f;
	};

protected:
	CMonster();
	~CMonster();

public:
	static CMonster* Create();
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;

public:
	void Die() override;
	void Change_State(const _uint _state);
	const MonsterStatus& GetStatus();

	const _float GetRadius() const;

protected:
	void CreateBody();
	void PaintTexture();
	void CreateAnimator();
	void CreateAI();

public:
	CNaviMeshAgent* GetNaviAgent();

	const _bool HasPendingHitReaction() const;
	const _bool IsHitReacting() const;
	void BeginHitReaction();
	void EndHitReaction();
	void TickHitReactionKnockback();

public:
	void GetHitHandler(const HurtDescription& _hurtDesc) override;

protected:
	_uint m_iCurrentState;
	vector<_bool> m_vMaterialTransparent;
	vector<CSkinnedMeshRenderer*> m_vMeshRenderers;
	CMonsterController* m_pController;

	MonsterStatus m_sStatus;
	_bool m_bHitRequested;
	_bool m_bHitReacting;
	vector3 m_vPendingHitKnockback;
	vector3 m_vActiveHitKnockback;
	quaternion m_qHitReactionRotation;
	_float m_fHitKnockbackRemainDistance;

	CMonsterHUD* m_pHUD;
};

