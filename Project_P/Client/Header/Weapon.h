#pragma once

#include "epch.h"

class CWeapon abstract : public CComponent
{
public:
	enum class WeaponType { Swords, Bow, Bomb };

protected:
	CWeapon();
	~CWeapon();

public:
	HRESULT Initialize() override;

private:
	CMeshRenderer* m_pRenderer;
	CTransform* m_pTargetHand;
};

