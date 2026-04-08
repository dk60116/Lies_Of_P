#include "cpch.h"
#include "Weapon.h"

CWeapon::CWeapon()
	: m_strWeaponName(L"")
	, m_vRenderers({})
	, m_pTargetHand(nullptr)
	, m_pHurtBox(nullptr)
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

	const wstring weaponFbxPath = L"Player/Weapon/" + m_strWeaponName + L".fbx";
	m_vRenderers = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnPath(weaponFbxPath), CResources::GetInstance().LoadSkinnedBonesOnPath(weaponFbxPath), 1.f, vector3::back() * 90.f);

	CTexture* weaponTD = CResources::GetInstance().LoadOnPath<CTexture>(L"Player/Weapon/Textures/CH_W_Sword_02_D.png");
	CTexture* weaponTN = CResources::GetInstance().LoadOnPath<CTexture>(L"Player/Weapon/Textures/CH_W_Sword_02_N.png");
	CTexture* weaponTM = CResources::GetInstance().LoadOnPath<CTexture>(L"Player/Weapon/Textures/CH_W_Sword_02_M.png");

	for (TRAVERSAL_ITER(m_vRenderers, it))
	{
		(*it)->Get_Material()->Set_Texture(weaponTD);
		(*it)->Get_Material()->Set_Texture(weaponTN, 1);
	}

	if (m_vRenderers.size() > 1)
		m_vRenderers[1]->Get_Material()->Set_Texture(weaponTM, 2);

	CreateHurtBox();

	return S_OK;
}

void CWeapon::OnDestroy()
{
}

void CWeapon::CreateHurtBox()
{
	m_pHurtBox = m_pGameObject->AddComponent<CHurtBox>();
}

void CWeapon::EnableHurtBox()
{
	m_pHurtBox->EnableBox();
}

void CWeapon::DisableHurtBox()
{
	m_pHurtBox->DisableBox();
}

void CWeapon::SetDamage(const _int _damage)
{
	if (m_pHurtBox)
		m_pHurtBox->SetDamage(_damage);
}

void CWeapon::SetKnockback(const _bool _knockback)
{
	if (m_pHurtBox)
		m_pHurtBox->SetKnockback(_knockback);
}

void CWeapon::SetKnockbackAmount(const _float _knockbackAmount)
{
	if (m_pHurtBox)
		m_pHurtBox->SetKnockbackAmount(_knockbackAmount);
}
