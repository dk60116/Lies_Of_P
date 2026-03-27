#pragma once
#include "ColliderBox.h"
#include <unordered_set>

class CHurtBox final : public CColliderBox
{
	friend class CGameObject;

protected:
	CHurtBox();
	~CHurtBox();

private:
	static CHurtBox* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnTriggerEnter(CCollider* _other) override;
	void OnTriggerStay(CCollider* _other) override;

public:
	void SetDamage(const _int damage);
	const _int GetDamage() const;
	void SetKnockback(const _bool _knockback);
	void SetKnockbackAmount(const _float _knockbackAmount);

private:
	void HandleHitOverlap(CCollider* _other);
	_bool TryRegisterHitTarget(CCharacter* _target);
	void OnHitEvent(CCharacter* _target);

protected:
	void OnBoxEnabled() override;
	void OnBoxDisabled() override;

private:
	HurtDescription m_sHurtDesc;
	CRigidBody* m_pRigid;
	std::unordered_set<size_t> m_setHitTargets;
};


