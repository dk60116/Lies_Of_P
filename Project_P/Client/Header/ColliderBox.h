#pragma once
#include "Component.h"

struct HurtDescription
{
	vector3 position = vector3::zero();
	vector3 forward = vector3::zero();
	_int damage = 0;
	_bool knockback = true;
	_float knockbackAmount = 1.f;
};

class CColliderBox abstract : public CComponent
{
public:
	enum class ColliderOwner { Player, Enemy, Npc };

protected:
	CColliderBox();
	~CColliderBox();

public:
	HRESULT Initialize() override;
	void LateUpdate() override;
	void OnDestroy() override;

public:
	void CreateHurtBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3& _size, const vector3& _center = vector3::zero());
	void CreateHitBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3& _size, const vector3& _center = vector3::zero());

public:
	CCharacter* GetCharacter();

	void EnableBox();
	void DisableBox();
	void RequestDisableBox();

protected:
	CCharacter* m_pCharacter;
	CCollider* m_pCollider;

	ColliderOwner m_eOwner;

	_bool m_bPendingDisable;
	_bool m_bColliderBootstrapped;
	wstring m_strCBName;

protected:
	virtual void OnBoxEnabled();
	virtual void OnBoxDisabled();
};

