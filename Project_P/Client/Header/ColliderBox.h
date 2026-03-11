#pragma once
#include "Component.h"

struct HurtDescription
{
	_int damage = 0;
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
	void CreateHurtBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3 _size);

public:
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
};

