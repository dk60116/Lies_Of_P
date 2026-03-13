#include "cpch.h"
#include "Eve_Sword.h"

CEve_Sword::CEve_Sword()
{
	m_strWeaponName = L"Eve_Sword";
}

CEve_Sword::~CEve_Sword()
{
}

CEve_Sword* CEve_Sword::Create()
{
	return new CEve_Sword();
}

CComponent* CEve_Sword::Clone() const
{
	CEve_Sword* clone = new CEve_Sword();

	return clone;
}

void CEve_Sword::CreateHurtBox()
{
	__super::CreateHurtBox();

	m_pHurtBox->CreateHurtBox(CGameManager::GetInstance().Get_Player(), L"Sword", CCollider::ColliderType::Box, vector3(60.f, 500.f, 20.f), vector3(-5.f, 150.f, 0.f));
}

void CEve_Sword::OnDestroy()
{
	__super::OnDestroy();
}
