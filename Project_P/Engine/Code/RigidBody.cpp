#include "epch.h"
#include "RigidBody.h"

CRigidBody::CRigidBody()
    : m_pCollider(nullptr)
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

    return clone;
}

void CRigidBody::OnDestroy()
{
}
