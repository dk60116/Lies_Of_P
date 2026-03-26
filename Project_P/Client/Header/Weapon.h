#pragma once

#include "epch.h"
#include "HurtBox.h"

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
	virtual void CreateHurtBox();

public:
	void EnableHurtBox();
	void DisableHurtBox();
	void SetDamage(const _int _damage);
	void SetKnockback(const _bool _knockback);

protected:
	wstring m_strWeaponName;
	vector<CSkinnedMeshRenderer*> m_vRenderers;
	CTransform* m_pTargetHand;
	CHurtBox* m_pHurtBox;
};

