#pragma once
#include "ColliderBox.h"

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

private:
	void OnHitEvent(CCharacter* _target);

private:
	HurtDescription m_sHurtDesc;
	CRigidBody* m_pRigid;
};


