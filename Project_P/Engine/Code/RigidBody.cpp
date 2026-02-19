#include "epch.h"
#include "RigidBody.h"

CRigidBody::CRigidBody()
    : m_pCollider(nullptr)
    , m_fMass(1.f)
    , m_bUseGravity(true)
    , m_bIsKinematic(false)
{
	m_strName = L"RigidBody";
}

CRigidBody::~CRigidBody()
{
}

CRigidBody* CRigidBody::Create()
{
    return new CRigidBody();
}

CComponent* CRigidBody::Clone() const
{
    CRigidBody* clone = new CRigidBody();
    clone->m_fMass = m_fMass;
    clone->m_bUseGravity = m_bUseGravity;
    clone->m_bIsKinematic = m_bIsKinematic;

    return clone;
}

void CRigidBody::OnDestroy()
{
}
