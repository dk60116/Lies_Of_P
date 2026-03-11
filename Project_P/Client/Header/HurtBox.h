#pragma once
#include "ColliderBox.h"

class CHurtBox final : public CColliderBox
{
	friend class CGameObject;

protected:
	CHurtBox();
	~CHurtBox();

public:
	void OnTriggerEnter(CCollider* _other) override;

private:
	static CHurtBox* Create();
	CComponent* Clone() const override;

public:
	void Awake() override;

private:
	void OnHitEvent(CCharacter* _target);

private:
	HurtDescription m_sHurtDesc;
};

