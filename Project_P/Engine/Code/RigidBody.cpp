#include "epch.h"
#include "RigidBody.h"
#include "Collider.h"

#include <algorithm>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace
{
    inline Vec3 ToJPHVec3(const vector3& v) { return Vec3(v.x, v.y, v.z); }

    inline void DecomposeWorldMatrix(const _matrix& m, Vec3& outPos, Quat& outRot)
    {
        XMVECTOR s, r, t;
        XMMatrixDecompose(&s, &r, &t, m);
        XMFLOAT3 p;
        XMStoreFloat3(&p, t);
        outPos = Vec3(p.x, p.y, p.z);

        XMFLOAT4 q;
        XMStoreFloat4(&q, r);
        outRot = Quat(q.x, q.y, q.z, q.w);
    }

    inline PhysicsSystem& GetPS()
    {
        return CPhysics::GetInstance().GetPhysicsSystem();
    }

    inline BodyInterface& GetBI()
    {
        return GetPS().GetBodyInterface();
    }
}

CRigidBody::CRigidBody()
    : m_lColliderList({})
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
    auto& componentList = m_pGameObject->Get_ComponentList();

    for (TRAVERSAL_ITER(componentList, it))
    {
        if (auto c = dynamic_cast<CCollider*>(*it))
        {
            m_lColliderList.push_back(c);
            c->AddRef();
        }
    }

    for (TRAVERSAL_ITER(componentList, it))
    {
        if (auto c = dynamic_cast<CCollider*>(*it))
            AddCollider(c);
    }

    return S_OK;
}

void CRigidBody::Awake()
{
    MarkBodyDirty();
    RebuildBodiesIfDirty();
}

void CRigidBody::FixedUpdate()
{
    RebuildBodiesIfDirty();
}

void CRigidBody::OnCollisionEnter(CCollider* _other)
{
}

void CRigidBody::OnCollisionStay(CCollider* _other)
{
}

void CRigidBody::OnCollisionExit(CCollider* other)
{
}

void CRigidBody::OnTriggerEnter(CCollider* other)
{
}

void CRigidBody::OnTriggerStay(CCollider* _other)
{
}

void CRigidBody::OnTriggerExit(CCollider* _other)
{
}

void CRigidBody::OnDestroy()
{
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
}
void CRigidBody::BuildCompoundShapes(const list<CCollider*>& _colliders, RefConst<Shape>& _outBodyCompound, RefConst<Shape>& _outSensorCompound)
{
    StaticCompoundShapeSettings bodySettings;
    StaticCompoundShapeSettings sensorSettings;

    bool hasBodyChild = false;
    bool hasSensorChild = false;

    for (CCollider* col : _colliders)
    {
        if (!col)
            continue;

        const Shape* childShape = col->GetShape();
        if (!childShape)
            continue;

        const Vec3 localCenter = ToJPHVec3(col->GetCenter());
        const Quat localRot = Quat::sIdentity();

        if (col->IsTrigger())
        {
            sensorSettings.AddShape(localCenter, localRot, childShape);
            hasSensorChild = true;
        }
        else
        {
            bodySettings.AddShape(localCenter, localRot, childShape);
            hasBodyChild = true;
        }
    }

    _outBodyCompound = nullptr;
    _outSensorCompound = nullptr;

    if (hasBodyChild)
    {
        ShapeSettings::ShapeResult r = bodySettings.Create();
        if (!r.HasError())
            _outBodyCompound = r.Get(); 
    }

    if (hasSensorChild)
    {
        ShapeSettings::ShapeResult r = sensorSettings.Create();
        if (!r.HasError())
            _outSensorCompound = r.Get();
    }
}

const BodyID CRigidBody::GetSensorBodyID() const
{
    return m_iSensorBodyID;
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
    }
}

void CRigidBody::RemvoeCollier(CCollider* _collider)
{
    auto it = find(m_lColliderList.begin(), m_lColliderList.end(), _collider);

    if (it == m_lColliderList.end())
        return;

    if (_collider)
    {
        m_lColliderList.remove(_collider);
        Safe_Release(_collider);
    }
}

void CRigidBody::MarkBodyDirty()
{
}

void CRigidBody::RebuildBodiesIfDirty()
{
    if (!m_bBodyDirty)
        return;

    DestroyBodies();

    RefConst<Shape> bodyCompound;
    RefConst<Shape> sensorCompound;
    BuildCompoundShapes(m_lColliderList, bodyCompound, sensorCompound);

    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

    // --- 일반 바디 생성 ---
    if (bodyCompound != nullptr)
    {
        // 보관용 raw ptr(refcount)
        m_pCompoundShape = bodyCompound.GetPtr();
        m_pCompoundShape->AddRef();

        const EMotionType motion = m_bKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
        const ObjectLayer layer = Layers::MOVING;

        BodyCreationSettings settings(m_pCompoundShape, pos, rot, motion, layer);

        if (!m_bKinematic)
        {
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = m_fMass;
        }

        Body* body = GetBI().CreateBody(settings);
        m_iBodyID = body->GetID();
        m_bHasBody = true;

        GetBI().SetUserData(m_iBodyID, (uint64)this);

        GetBI().AddBody(m_iBodyID, EActivation::Activate);
    }

    if (sensorCompound != nullptr)
    {
        m_pSensorCompoundShape = sensorCompound.GetPtr();
        m_pSensorCompoundShape->AddRef();

        const EMotionType motion = m_bKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
        const ObjectLayer layer = Layers::SENSOR;

        BodyCreationSettings settings(m_pSensorCompoundShape, pos, rot, motion, layer);
        settings.mIsSensor = true;

        Body* body = GetBI().CreateBody(settings);
        m_iSensorBodyID = body->GetID();
        m_bHasSensorBody = true;

        GetBI().SetUserData(m_iSensorBodyID, (uint64)this);
        GetBI().AddBody(m_iSensorBodyID, EActivation::Activate);
    }

    m_bBodyDirty = false;
}

void CRigidBody::DestroyBodies()
{
}

void CRigidBody::SyncKinematicToJolt()
{
}

void CRigidBody::SyncDynamicFromJolt()
{
}
