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

void CEve_Sword::OnDestroy()
{
	__super::OnDestroy();
}
