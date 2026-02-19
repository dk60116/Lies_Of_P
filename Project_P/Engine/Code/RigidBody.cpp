#include "epch.h"
#include "RigidBody.h"
#include "Collider.h"
#ifndef _CLIENT_BUILD
#include "Editor.h"
#include "ImGuizmo.h"
#endif

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
    , m_bUseGravity(true)
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
    clone->m_bUseGravity = m_bUseGravity;
    clone->m_fMass = m_fMass;

    return clone;
}

HRESULT CRigidBody::Initialize()
{
    auto& componentList = m_pGameObject->Get_ComponentList();

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

        if (_other && _other->Get_GameObject())
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
    DestroyBodies();

    for (TRAVERSAL_ITER(m_lColliderList, it))
    {
        if (*it)
            (*it)->SetRigidBody(nullptr);

        Safe_Release(*it);
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

const BodyID CRigidBody::GetBodyID() const
{
    return m_iBodyID;
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
        _collider->SetRigidBody(this);
        m_bBodyDirty = true;
    }
}

void CRigidBody::RemvoeCollier(CCollider* _collider)
{
    auto it = find(m_lColliderList.begin(), m_lColliderList.end(), _collider);

    if (it == m_lColliderList.end())
        return;

    if (_collider)
    {
        _collider->SetRigidBody(nullptr);
        m_lColliderList.remove(_collider);
        Safe_Release(_collider);
        m_bBodyDirty = true;
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


_bool CRigidBody::IsUseGravity() const
{
    return m_bUseGravity;
}

void CRigidBody::SetUseGravity(_bool _useGravity)
{
    if (m_bUseGravity == _useGravity)
        return;

    m_bUseGravity = _useGravity;

    const EMotionType motion = m_bKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;

    if (m_bHasBody)
    {
        GetBI().SetGravityFactor(m_iBodyID, motion == EMotionType::Dynamic && m_bUseGravity ? 1.f : 0.f);
        if (motion == EMotionType::Dynamic && m_bUseGravity)
            GetBI().ActivateBody(m_iBodyID);
        if (motion == EMotionType::Dynamic && !m_bUseGravity)
            GetBI().SetLinearAndAngularVelocity(m_iBodyID, Vec3::sZero(), Vec3::sZero());
    }

    if (m_bHasSensorBody)
    {
        GetBI().SetGravityFactor(m_iSensorBodyID, motion == EMotionType::Dynamic && m_bUseGravity ? 1.f : 0.f);
        if (motion == EMotionType::Dynamic && m_bUseGravity)
            GetBI().ActivateBody(m_iSensorBodyID);
        if (motion == EMotionType::Dynamic && !m_bUseGravity)
            GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, Vec3::sZero(), Vec3::sZero());
    }
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
        GetBI().SetGravityFactor(m_iBodyID, motion == EMotionType::Dynamic && m_bUseGravity ? 1.f : 0.f);

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
        GetBI().SetGravityFactor(m_iSensorBodyID, motion == EMotionType::Dynamic && m_bUseGravity ? 1.f : 0.f);
        GetBI().AddBody(m_iSensorBodyID, EActivation::Activate);
    }

    m_bBodyDirty = false;
}

void CRigidBody::DestroyBodies()
{
    if (m_bHasBody)
    {
        GetBI().RemoveBody(m_iBodyID);
        GetBI().DestroyBody(m_iBodyID);
        m_iBodyID = BodyID();
        m_bHasBody = false;
    }

    if (m_bHasSensorBody)
    {
        GetBI().RemoveBody(m_iSensorBodyID);
        GetBI().DestroyBody(m_iSensorBodyID);
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
    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

    if (m_bHasBody)
    {
        GetBI().SetPositionAndRotation(m_iBodyID, RVec3(pos), rot, EActivation::DontActivate);
        GetBI().SetLinearAndAngularVelocity(m_iBodyID, Vec3::sZero(), Vec3::sZero());
    }

    if (m_bHasSensorBody)
    {
        GetBI().SetPositionAndRotation(m_iSensorBodyID, RVec3(pos), rot, EActivation::DontActivate);
        GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, Vec3::sZero(), Vec3::sZero());
    }
}

void CRigidBody::SyncDynamicFromJolt()
{
    if (!m_bHasBody && !m_bHasSensorBody)
        return;

    const BodyID sourceBodyId = m_bHasBody ? m_iBodyID : m_iSensorBodyID;

    Vec3 tfPos;
    Quat tfRot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), tfPos, tfRot);

    _bool forceTransformOverride = false;
#ifndef _CLIENT_BUILD
    CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject();
    if (selected != nullptr && selected == m_pGameObject)
        forceTransformOverride = ImGuizmo::IsUsing();
#endif

    const RVec3 joltPos = GetBI().GetPosition(sourceBodyId);
    const Quat joltRot = GetBI().GetRotation(sourceBodyId);

    const _bool transformOverridden = forceTransformOverride;

    if (transformOverridden)
    {
        const RVec3 targetPos(tfPos.GetX(), tfPos.GetY(), tfPos.GetZ());
        const Quat targetRot = tfRot;

        if (m_bHasBody)
        {
            GetBI().SetPositionAndRotation(m_iBodyID, targetPos, targetRot, EActivation::Activate);
            GetBI().SetLinearAndAngularVelocity(m_iBodyID, Vec3::sZero(), Vec3::sZero());
        }

        if (m_bHasSensorBody)
        {
            GetBI().SetPositionAndRotation(m_iSensorBodyID, targetPos, targetRot, EActivation::Activate);
            GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, Vec3::sZero(), Vec3::sZero());
        }

        return;
    }

    vector3 pos = vector3(static_cast<_float>(joltPos.GetX()), static_cast<_float>(joltPos.GetY()), static_cast<_float>(joltPos.GetZ()));
    quaternion rot = quaternion(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());

    Get_Transform()->Set_Position(pos);
    Get_Transform()->Set_Quaternion(rot);

    if (m_bHasBody && m_bHasSensorBody)
    {
        GetBI().SetPositionAndRotation(m_iSensorBodyID, joltPos, joltRot, EActivation::DontActivate);
        GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, GetBI().GetLinearVelocity(m_iBodyID), GetBI().GetAngularVelocity(m_iBodyID));
    }
}
