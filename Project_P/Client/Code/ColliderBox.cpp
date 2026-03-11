#include "cpch.h"
#include "ColliderBox.h"

CColliderBox::CColliderBox()
	: m_pCharacter(nullptr)
	, m_pCollider(nullptr)
	, m_strCBName(L"")
{
}

CColliderBox::~CColliderBox()
{
}

HRESULT CColliderBox::Initialize()
{
	return S_OK;
}

void CColliderBox::OnDestroy()
{
	Safe_Release(m_pCharacter);
}

void CColliderBox::CreateHurtBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3 _size)
{
	m_pCharacter = _character;

	m_strCBName = _name;

	if (m_pCharacter)
		m_pCharacter->AddRef();

	switch (_type)
	{
	case CCollider::ColliderType::Box:
		m_pCollider = m_pGameObject->AddComponent<CBoxCollider>();
		dynamic_cast<CBoxCollider*>(m_pCollider)->SetSize(_size);
		break;
	case CCollider::ColliderType::Sphere:
		m_pCollider = m_pGameObject->AddComponent<CSphereCollider>();
		dynamic_cast<CSphereCollider*>(m_pCollider)->SetRadius((_size.x + _size.y + _size.z) / 3.f);
		break;
	case CCollider::ColliderType::Capsule:
		m_pCollider = m_pGameObject->AddComponent<CCapsuleCollider>();
		dynamic_cast<CCapsuleCollider*>(m_pCollider)->SetHeight(_size.y);
		dynamic_cast<CCapsuleCollider*>(m_pCollider)->SetRadius((_size.x + _size.y) / 2.f);
		break;
	case CCollider::ColliderType::Mesh:
		m_pCollider = m_pGameObject->AddComponent<CMeshCollider>();
		break;
	}

	m_pCollider->SetTrigger(true);
	m_pCollider->Set_Enable(false);
}

void CColliderBox::EnableBox()
{
	m_pCollider->Set_Enable(true);
}

void CColliderBox::DisableBox()
{
	m_pCollider->Set_Enable(false);
}
