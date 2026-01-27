#include "cpch.h"
#include "Weapon.h"

CWeapon::CWeapon()
	: m_pRenderer(nullptr)
	, m_pTargetHand(nullptr)
{
}

CWeapon::~CWeapon()
{
	Safe_Release(m_pTargetHand);
}

HRESULT CWeapon::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL; 

	return S_OK;
}
