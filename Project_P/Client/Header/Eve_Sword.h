#pragma once
#include "Weapon.h"

class CEve_Sword final : public CWeapon
{
	friend class CGameObject;

protected:
	CEve_Sword();
	~CEve_Sword();

protected:
	static CEve_Sword* Create();
	CComponent* Clone() const override;

protected:
	void CreateHurtBox() override;

public:
	void OnDestroy() override;
};

