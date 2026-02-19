#include "epch.h"
#include "RigidBody.h"
#include "Collider.h"

#include <algorithm>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace
{
    // =========================
    // TODO: 엔진 수학 타입 변환
    // =========================
    inline Vec3 ToJPHVec3(const vector3& v)
    {
        return Vec3(v.x, v.y, v.z);
    }

    // 만약 quaternion 타입이 있다면 여기서 변환해 주세요.
    // inline Quat ToJPHQuat(const quaternion& q) { return Quat(q.x, q.y, q.z, q.w); }

    inline void ReleaseShapePtr(const Shape*& s)
    {
        if (s)
        {
            s->Release();
            s = nullptr;
        }
    }

    inline PhysicsSystem& GetPhysicsSystem()
    {
        // 예: return CPhysics::GetInstance().GetJoltSystem();
        extern PhysicsSystem& GGetJoltPhysicsSystem(); 
        return GGetJoltPhysicsSystem();
    }

    inline BodyInterface& GetBodyInterface()
    {
        return GetPhysicsSystem().GetBodyInterface();
    }

    inline ObjectLayer GetDefaultObjectLayer(_bool /*isSensor*/, _bool /*isKinematic*/)
    {
        return ObjectLayer(0);
    }

    inline EMotionType GetMotionType(bool isKinematic)
    {
        return isKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
    }

    inline EActivation ActivateMode()
    {
        return EActivation::Activate;
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

    return S_OK;
}

void CRigidBody::Awake()
{
    auto& componentList = m_pGameObject->Get_ComponentList();

    for (TRAVERSAL_ITER(componentList, it))
        if (auto c = dynamic_cast<CCollider*>(*it))
            AddCollider(c);
}

void CRigidBody::FixedUpdate()
{
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

        const Shape* childShape = col->GetShape(); // <- CCollider에서 dirty 처리/shape 생성이 되어 있어야 함
        if (!childShape)
            continue;

        const Vec3 localCenter = ToJPHVec3(col->GetCenter());
        const Quat localRot = Quat::sIdentity(); // 필요하면 콜라이더 로컬 회전도 지원

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
