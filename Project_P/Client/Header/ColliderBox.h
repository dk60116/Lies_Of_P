#pragma once
#include "Component.h"

class CColliderBox abstract : public CComponent
{
protected:
	CColliderBox();
	~CColliderBox();

public:
	HRESULT Initialize() override;
	void OnDestroy() override;

public:
	void CreateHurtBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3 _size);

public:
	void EnableBox();
	void DisableBox();

protected:
	CCharacter* m_pCharacter;
	CCollider* m_pCollider;

	wstring m_strCBName;
};

