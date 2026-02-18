#include "epch.h"
#include "RigidBody.h"
#include "Collider.h"

#include <algorithm>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

CRigidBody::CRigidBody()
    : m_lColliderList({})
    , m_ColliderContactRefCounts({})
    , m_iContactPairCount(0)
    , m_bBodyDirty(false)
    , m_iBodyID(0)
    , m_bHasBody(false)
    , m_iSensorBodyID(0)
    , m_bHasSensorBody(false)
    , m_pCompoundShape(nullptr)
    , m_pSensorCompoundShape(nullptr)
    , m_bKinematic(false)
    , m_fMass(1.f)
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
    clone->m_bKinematic = m_bKinematic;
    clone->m_fMass = m_fMass;
    return clone;
}

HRESULT CRigidBody::Initialize()
{
    return S_OK;
}

void CRigidBody::Awake()
{
    CColliderManager::GetInstance().RegisterRigidBody(this);

    auto& componentList = m_pGameObject->Get_ComponentList();

    for (TRAVERSAL_ITER(componentList, it))
        if (auto c = dynamic_cast<CCollider*>(*it))
            AddCollider(c);

    MarkBodyDirty();
}

void CRigidBody::FixedUpdate()
{
    RebuildBodiesIfNeeded();

    if (m_bHasSensorBody)
    {
        auto& sys = CColliderManager::GetInstance().GetSystem();
        BodyInterface& bi = sys.GetBodyInterface();

        auto& tr = *m_pGameObject->Get_Transform();
        const vector3 pos = tr.Get_Position();
        const quaternion rot = tr.Get_Quaternion();

        const JPH::RVec3 jpos(pos.x, pos.y, pos.z);
        const JPH::Quat  jrot(rot.x, rot.y, rot.z, rot.w);

        bi.SetPositionAndRotation(m_iSensorBodyID, jpos, jrot, EActivation::DontActivate);
    }
}

void CRigidBody::OnCollisionEnter(CCollider* _other)
{
    UpdateContactState(true);
}

void CRigidBody::OnCollisionStay(CCollider* _other)
{
}

void CRigidBody::OnCollisionExit(CCollider* other)
{
    UpdateContactState(false);
}

void CRigidBody::OnTriggerEnter(CCollider* other)
{
    UpdateContactState(true);
}

void CRigidBody::OnTriggerStay(CCollider* _other)
{
}

void CRigidBody::OnTriggerExit(CCollider* _other)
{
    UpdateContactState(false);
}

void CRigidBody::OnDestroy()
{
    DestroyBodies();
    ClearContactState();

    for (TRAVERSAL_ITER(m_lColliderList, it))
        Safe_Release(*it);

    if (m_pCompoundShape) 
    { 
        m_pCompoundShape->Release(); 
        m_pCompoundShape = nullptr; 
    }
    if (m_pSensorCompoundShape) 
    { 
        m_pSensorCompoundShape->Release();
        m_pSensorCompoundShape = nullptr; 
    }

    CColliderManager::GetInstance().UnregisterRigidBody(this);
}

const BodyID CRigidBody::GetBodyID() const
{
    return m_iBodyID;
}

const BodyID CRigidBody::GetSensorBodyID() const
{
    return m_iSensorBodyID;
}

void CRigidBody::MarkBodyDirty()
{
    m_bBodyDirty = true;
}

void CRigidBody::RebuildBodiesIfNeeded()
{
    if (!m_bBodyDirty)
        return;

    auto& sys = CColliderManager::GetInstance().GetSystem();
    BodyInterface& bi = sys.GetBodyInterface();

    ClearContactState();
    DestroyBodies();

    if (m_pCompoundShape) 
    { 
        m_pCompoundShape->Release();
        m_pCompoundShape = nullptr;
    }
    if (m_pSensorCompoundShape) 
    {
        m_pSensorCompoundShape->Release();
        m_pSensorCompoundShape = nullptr;
    }

    m_pCompoundShape = BuildCompoundShape(false);
    m_pSensorCompoundShape = BuildCompoundShape(true);

    CTransform* tr = m_pGameObject->Get_Transform();
    const vector3 pos = tr->Get_Position();
    const quaternion rot = tr->Get_Quaternion();

    const RVec3 jpos(pos.x, pos.y, pos.z);
    const Quat  jrot(rot.x, rot.y, rot.z, rot.w);

    if (m_pCompoundShape)
    {
        const EMotionType motion = m_bKinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        const ObjectLayer layer = m_bKinematic ? Layers::MOVING : Layers::MOVING;

        BodyCreationSettings bcs(m_pCompoundShape, jpos, jrot, motion, layer);

        Body* body = bi.CreateBody(bcs);
        if (body)
        {
            m_iBodyID = body->GetID();
            body->SetUserData(reinterpret_cast<uint64_t>(this));

            bi.AddBody(m_iBodyID, JPH::EActivation::Activate);
            m_bHasBody = true;
        }
    }

    if (m_pSensorCompoundShape)
    {
        BodyCreationSettings sbcs(m_pSensorCompoundShape, jpos, jrot, EMotionType::Kinematic, Layers::SENSOR);
        sbcs.mIsSensor = true; 

        Body* sbody = bi.CreateBody(sbcs);
        if (sbody)
        {
            m_iSensorBodyID = sbody->GetID();
            sbody->SetUserData(reinterpret_cast<uint64_t>(this));

            bi.AddBody(m_iSensorBodyID, JPH::EActivation::Activate);
            m_bHasSensorBody = true;
        }
    }

    m_bBodyDirty = false;
}

void CRigidBody::DestroyBodies()
{
    auto& sys = CColliderManager::GetInstance().GetSystem();
    BodyInterface& bi = sys.GetBodyInterface();

    if (m_bHasBody)
    {
        bi.RemoveBody(m_iBodyID);
        bi.DestroyBody(m_iBodyID);
        m_iBodyID = BodyID();
        m_bHasBody = false;
    }

    if (m_bHasSensorBody)
    {
        bi.RemoveBody(m_iSensorBodyID);
        bi.DestroyBody(m_iSensorBodyID);
        m_iSensorBodyID = BodyID();
        m_bHasSensorBody = false;
    }
}

const Shape* CRigidBody::BuildCompoundShape(bool trigger_only)
{
    StaticCompoundShapeSettings compound;
    _bool added = false;

    for (CCollider* c : m_lColliderList)
    {
        if (!c) 
            continue;
        if (c->IsTrigger() != trigger_only) 
            continue;

        c->BuildShapeIfNeeded();

        const Shape* s = c->GetShape();
        if (!s)
            continue;

        compound.AddShape(Vec3::sZero(), Quat::sIdentity(), s);
        added = true;
    }

    if (!added)
        return nullptr;

    auto res = compound.Create();
    if (res.HasError())
        return nullptr;

    Ref<Shape> ref = res.Get();
    const Shape* shape = ref.GetPtr();
    shape->AddRef();

    return shape;
}


void CRigidBody::UpdateContactState(const _bool entering)
{
    if (entering)
    {
        ++m_iContactPairCount;
        for (CCollider* collider : m_lColliderList)
        {
            if (!collider)
                continue;

            _int& count = m_ColliderContactRefCounts[collider];
            ++count;
            collider->SetInContact(true);
        }
        return;
    }

    m_iContactPairCount = max(0, m_iContactPairCount - 1);

    for (CCollider* collider : m_lColliderList)
    {
        if (!collider)
            continue;

        auto it = m_ColliderContactRefCounts.find(collider);
        if (it == m_ColliderContactRefCounts.end())
            continue;

        it->second = max(0, it->second - 1);
        collider->SetInContact(it->second > 0);
    }
}

void CRigidBody::ClearContactState()
{
    m_iContactPairCount = 0;

    for (CCollider* collider : m_lColliderList)
    {
        if (!collider)
            continue;

        collider->SetInContact(false);
    }

    m_ColliderContactRefCounts.clear();
}
void CRigidBody::AddCollider(CCollider* _collider)
{
    auto it = find(m_lColliderList.begin(), m_lColliderList.end(), _collider);

    if (it != m_lColliderList.end())
        return;

    if (_collider)
    {
        m_lColliderList.push_back(_collider);
        m_lColliderList.back()->AddRef();
        m_ColliderContactRefCounts[_collider] = 0;
        _collider->SetInContact(false);
    }

    MarkBodyDirty();
}

void CRigidBody::RemvoeCollier(CCollider* _collider)
{
    auto it = find(m_lColliderList.begin(), m_lColliderList.end(), _collider);

    if (it == m_lColliderList.end())
        return;

    if (_collider)
    {
        _collider->SetInContact(false);
        m_ColliderContactRefCounts.erase(_collider);
        m_lColliderList.remove(_collider);
        Safe_Release(_collider);
    }

    MarkBodyDirty();
}
