#pragma once

#include "epch.h"

class CWeapon abstract : public CComponent
{
public:
	enum class WeaponType { Swords };

protected:
	CWeapon();
	~CWeapon();

public:
	HRESULT Initialize() override;
	void OnDestroy() override;

protected:
	wstring m_strWeaponName;
	vector<CSkinnedMeshRenderer*> m_vRenderers;
	CTransform* m_pTargetHand;
};

