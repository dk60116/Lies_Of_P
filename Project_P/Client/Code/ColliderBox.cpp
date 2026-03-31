#include "cpch.h"
#include "ColliderBox.h"
#include "RigidBody.h"

namespace
{
	void RefreshColliderPhysics(CGameObject* _gameObject)
	{
		if (!_gameObject)
			return;

		if (CRigidBody* rigidBody = _gameObject->GetComponent<CRigidBody>())
		{
			// Defer body rebuild to the engine's normal fixed-update pass.
			// Rebuilding immediately here can re-enter Jolt while contact callbacks are running.
			rigidBody->MarkBodyDirty();
		}
	}
}

CColliderBox::CColliderBox()
	: m_pCharacter(nullptr)
	, m_eOwner(ColliderOwner::Player)
	, m_pCollider(nullptr)
	, m_bPendingDisable(false)
	, m_bColliderBootstrapped(false)
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

void CColliderBox::LateUpdate()
{
	if (!m_bPendingDisable || !m_pCollider)
		return;

	m_bPendingDisable = false;
	const _bool wasEnabled = m_pCollider->Get_Enable();
	m_pCollider->SetEnable(false);
	if (wasEnabled)
		OnBoxDisabled();
	RefreshColliderPhysics(m_pGameObject);
}

void CColliderBox::OnDestroy()
{
	Safe_Release(m_pCharacter);
}

void CColliderBox::CreateHurtBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3& _size, const vector3& _center)
{
	m_pCharacter = _character;

	if (dynamic_cast<CPlayer*>(m_pCharacter))
	{
		m_eOwner = ColliderOwner::Player;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HurtBox_Player"));
	}
	else if (dynamic_cast<CMonster*>(m_pCharacter))
	{
		m_eOwner = ColliderOwner::Enemy;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HurtBox_Ememy"));
	}
	else
	{
		m_eOwner = ColliderOwner::Npc;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HurtBox_NPC"));
	}

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

	m_pCollider->SetCenter(_center);

	m_pCollider->SetTrigger(true);
	m_bPendingDisable = false;
	m_pCollider->SetEnable(false);
	RefreshColliderPhysics(m_pGameObject);
}

void CColliderBox::CreateHitBox(CCharacter* _character, const wstring& _name, const CCollider::ColliderType _type, const vector3& _size, const vector3& _center)
{
	m_pCharacter = _character;

	if (dynamic_cast<CPlayer*>(m_pCharacter))
	{
		m_eOwner = ColliderOwner::Player;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HitBox_Player"));
		m_pGameObject->SetTag(L"PlayerBody");
	}
	else if (dynamic_cast<CMonster*>(m_pCharacter))
	{
		m_eOwner = ColliderOwner::Enemy;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HitBox_Enemy"));
		m_pGameObject->SetTag(L"MonsterBody");
	}
	else
	{
		m_eOwner = ColliderOwner::Npc;
		m_pGameObject->SetLayer(CSceneManager::GetInstance().NameToLayer(L"HitBox_NPC"));
		m_pGameObject->SetTag(L"NPCBody");
	}

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

	m_pCollider->SetCenter(_center);

	m_pCollider->SetTrigger(true);
	m_bPendingDisable = false;
	m_pCollider->SetEnable(true);
	RefreshColliderPhysics(m_pGameObject);
}

CCharacter* CColliderBox::GetCharacter()
{
	return m_pCharacter;
}

void CColliderBox::EnableBox()
{
	if (!m_pCollider)
		return;

	const _bool wasEnabled = m_pCollider->Get_Enable();
	m_bPendingDisable = false;
	m_pCollider->SetEnable(true);
	if (!wasEnabled)
		OnBoxEnabled();
	RefreshColliderPhysics(m_pGameObject);
}

void CColliderBox::DisableBox()
{
	if (!m_pCollider)
		return;

	const _bool wasEnabled = m_pCollider->Get_Enable();
	m_bPendingDisable = false;
	m_pCollider->SetEnable(false);
	if (wasEnabled)
		OnBoxDisabled();
	RefreshColliderPhysics(m_pGameObject);
}

void CColliderBox::RequestDisableBox()
{
	if (!m_pCollider)
		return;

	m_bPendingDisable = true;
}

void CColliderBox::OnBoxEnabled()
{
}

void CColliderBox::OnBoxDisabled()
{
}
