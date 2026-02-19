#include "epch.h"
#include "RigidBody.h"

CRigidBody::CRigidBody()
    : m_pCollider(nullptr)
    , m_fMass(1.f)
    , m_fLinearDrag(0.f)
    , m_fAngularDrag(0.05f)
    , m_bUseGravity(true)
    , m_bIsKinematic(false)
{
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
    clone->m_fLinearDrag = m_fLinearDrag;
    clone->m_fAngularDrag = m_fAngularDrag;
    clone->m_bUseGravity = m_bUseGravity;
    clone->m_bIsKinematic = m_bIsKinematic;

    return clone;
}

void CRigidBody::OnDestroy()
{
}
