#pragma once
#include "Weapon.h"

class CEve_Sword final : public CWeapon
{
protected:
	CEve_Sword();
	~CEve_Sword();

public:
	static CEve_Sword* Create();
	CComponent* Clone() const override;

public:
	void OnDestroy() override;
};

