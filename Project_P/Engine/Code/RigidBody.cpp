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

    inline _matrix ToWorldMatrix(const RVec3& p, const Quat& q)
    {
        const _matrix S = XMMatrixScaling(1.f, 1.f, 1.f);
        const _matrix R = XMMatrixRotationQuaternion(XMVectorSet(q.GetX(), q.GetY(), q.GetZ(), q.GetW()));
        const _matrix T = XMMatrixTranslation(static_cast<_float>(p.GetX()), static_cast<_float>(p.GetY()), static_cast<_float>(p.GetZ()));
        return S * R * T;
    }

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
    , m_bUseGravity(true)
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

    if (m_bKinematic)
        SyncKinematicToJolt();
    else
        SyncDynamicFromJolt();
}

void CRigidBody::OnCollisionEnter(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        CDebug::LogError(L"ColEnter: " + _other->Get_GameObject()->Get_ObjectName());

        component->OnCollisionEnter(_other);
    }
}

void CRigidBody::OnCollisionStay(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        component->OnCollisionStay(_other);
    }
}

void CRigidBody::OnCollisionExit(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        component->OnCollisionExit(_other);
    }
}

void CRigidBody::OnTriggerEnter(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        component->OnTriggerEnter(_other);
    }
}

void CRigidBody::OnTriggerStay(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        component->OnTriggerStay(_other);
    }
}

void CRigidBody::OnTriggerExit(CCollider* _other)
{
    if (!m_pGameObject)
        return;

    auto& components = m_pGameObject->Get_ComponentList();
    for (TRAVERSAL_ITER(components, it))
    {
        CComponent* component = *it;
        if (!component || component == this || !component->Get_Enable())
            continue;

        component->OnTriggerExit(_other);
    }
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


_bool CRigidBody::IsKinematic() const
{
    return m_bKinematic;
}

void CRigidBody::SetKinematic(_bool _kinematic)
{
    if (m_bKinematic == _kinematic)
        return;

    m_bKinematic = _kinematic;
    m_bBodyDirty = true;
}

void CRigidBody::SetUseGravity(_bool _value)
{
    m_bUseGravity = _value;
}

_float CRigidBody::GetMass() const
{
    return m_fMass;
}

void CRigidBody::SetMass(_float _mass)
{
    const _float clampedMass = max(_mass, 0.001f);
    if (m_fMass == clampedMass)
        return;

    m_fMass = clampedMass;
    m_bBodyDirty = true;
}


CCollider* CRigidBody::GetEventCollider(_bool _triggerEvent) const
{
    for (CCollider* collider : m_lColliderList)
    {
        if (!collider)
            continue;

        if (collider->IsTrigger() == _triggerEvent)
            return collider;
    }

    for (CCollider* collider : m_lColliderList)
    {
        if (collider)
            return collider;
    }

    return nullptr;
}

void CRigidBody::MarkBodyDirty()
{
    m_bBodyDirty = true;
}

_bool CRigidBody::UseGraviry()
{
    return m_bUseGravity;
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

    // --- ÀÏ¹Ý ¹Ùµð »ý¼º ---
    if (bodyCompound != nullptr)
    {
        // º¸°ü¿ë raw ptr(refcount)
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
    BodyInterface& bodyInterface = GetBI();

    if (m_bHasBody)
    {
        if (bodyInterface.IsAdded(m_iBodyID))
            bodyInterface.RemoveBody(m_iBodyID);

        bodyInterface.DestroyBody(m_iBodyID);
        m_iBodyID = BodyID();
        m_bHasBody = false;
    }

    if (m_bHasSensorBody)
    {
        if (bodyInterface.IsAdded(m_iSensorBodyID))
            bodyInterface.RemoveBody(m_iSensorBodyID);

        bodyInterface.DestroyBody(m_iSensorBodyID);
        m_iSensorBodyID = BodyID();
        m_bHasSensorBody = false;
    }

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

void CRigidBody::SyncKinematicToJolt()
{
    const _matrix world = Get_Transform()->Get_WorldMatrix();
    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(world, pos, rot);

    BodyInterface& bodyInterface = GetBI();

    if (m_bHasBody)
    {
        bodyInterface.SetPositionAndRotation(m_iBodyID, RVec3(pos.GetX(), pos.GetY(), pos.GetZ()), rot, EActivation::Activate);
        bodyInterface.SetLinearAndAngularVelocity(m_iBodyID, Vec3::sZero(), Vec3::sZero());
    }

    if (m_bHasSensorBody)
    {
        bodyInterface.SetPositionAndRotation(m_iSensorBodyID, RVec3(pos.GetX(), pos.GetY(), pos.GetZ()), rot, EActivation::Activate);
        bodyInterface.SetLinearAndAngularVelocity(m_iSensorBodyID, Vec3::sZero(), Vec3::sZero());
    }
}

void CRigidBody::SyncDynamicFromJolt()
{
    if (!m_bHasBody)
        return;

    const BodyLockInterfaceLocking& lockInterface = GetPS().GetBodyLockInterface();
    BodyLockRead lock(lockInterface, m_iBodyID);
    if (!lock.Succeeded())
        return;

    const Body& body = lock.GetBody();
    const _matrix world = ToWorldMatrix(body.GetPosition(), body.GetRotation());
    Get_Transform()->SetTransformForMatrix(world);

    if (m_bHasSensorBody)
    {
        BodyInterface& bodyInterface = GetBI();
        bodyInterface.SetPositionAndRotation(m_iSensorBodyID, body.GetPosition(), body.GetRotation(), EActivation::DontActivate);
    }
}
