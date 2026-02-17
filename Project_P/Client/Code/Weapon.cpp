#include "cpch.h"
#include "Weapon.h"

CWeapon::CWeapon()
	: m_strWeaponName(L"")
	, m_vRenderers({})
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

	m_vRenderers = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(m_strWeaponName + L"_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(m_strWeaponName + L" (MeshBuffer)"), 0.01f, vector3::back() * 90.f);

	CTexture* weaponTD = CResources::GetInstance().LoadOnScene<CTexture>(m_strWeaponName + L"_TD (Texture)");
	CTexture* weaponTN = CResources::GetInstance().LoadOnScene<CTexture>(m_strWeaponName + L"_TN (Texture)");
	CTexture* weaponTM = CResources::GetInstance().LoadOnScene<CTexture>(m_strWeaponName + L"_TM (Texture)");

	for (TRAVERSAL_ITER(m_vRenderers, it))
	{
		(*it)->Get_Material()->Set_Texture(weaponTD);
		(*it)->Get_Material()->Set_Texture(weaponTN, 1);
	}

	m_vRenderers[1]->Get_Material()->Set_Texture(weaponTM, 2);

	return S_OK;
}

void CWeapon::OnDestroy()
{
}
