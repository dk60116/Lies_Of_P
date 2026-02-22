#include "epch.h"
#include "RigidBody.h"
#include "Collider.h"
#include "Physics.h"
#ifndef _CLIENT_BUILD
#include "Editor.h"
#include "ImGuizmo.h"
#endif

#include <algorithm>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyFilter.h> /

namespace
{
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
    , m_bUseGravity(false)
    , m_fMass(1.f)
    , m_bConstPositionX(false)
    , m_bConstPositionY(false)
    , m_bConstPositionZ(false)
    , m_bConstRotationX(false)
    , m_bConstRotationY(false)
    , m_bConstRotationZ(false)
    , m_vConstPosition(vector3::zero())
    , m_vConstRotation(vector3::zero())
    , m_bHasLastSyncedTransform(false)
    , m_vLastSyncedPosition(vector3::zero())
    , m_vLastSyncedRotation(quaternion::identity())
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
    clone->m_bConstPositionX = m_bConstPositionX;
    clone->m_bConstPositionY = m_bConstPositionY;
    clone->m_bConstPositionZ = m_bConstPositionZ;
    clone->m_bConstRotationX = m_bConstRotationX;
    clone->m_bConstRotationY = m_bConstRotationY;
    clone->m_bConstRotationZ = m_bConstRotationZ;
    clone->m_vConstPosition = m_vConstPosition;
    clone->m_vConstRotation = m_vConstRotation;

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

        const Vec3 localCenter = Vec3::sZero();
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


_bool CRigidBody::IsConstPositionX() const { return m_bConstPositionX; }
void CRigidBody::SetConstPositionX(_bool _value)
{
    if (m_bConstPositionX == _value)
        return;

    m_bConstPositionX = _value;
    if (_value)
        m_vConstPosition.x = Get_Transform()->Get_Position().x;
}

_bool CRigidBody::IsConstPositionY() const { return m_bConstPositionY; }
void CRigidBody::SetConstPositionY(_bool _value)
{
    if (m_bConstPositionY == _value)
        return;

    m_bConstPositionY = _value;
    if (_value)
        m_vConstPosition.y = Get_Transform()->Get_Position().y;
}

_bool CRigidBody::IsConstPositionZ() const { return m_bConstPositionZ; }
void CRigidBody::SetConstPositionZ(_bool _value)
{
    if (m_bConstPositionZ == _value)
        return;

    m_bConstPositionZ = _value;
    if (_value)
        m_vConstPosition.z = Get_Transform()->Get_Position().z;
}

_bool CRigidBody::IsConstRotationX() const { return m_bConstRotationX; }
void CRigidBody::SetConstRotationX(_bool _value)
{
    if (m_bConstRotationX == _value)
        return;

    m_bConstRotationX = _value;
    if (_value)
        m_vConstRotation.x = Get_Transform()->Get_EulerAngles().x;
}

_bool CRigidBody::IsConstRotationY() const { return m_bConstRotationY; }
void CRigidBody::SetConstRotationY(_bool _value)
{
    if (m_bConstRotationY == _value)
        return;

    m_bConstRotationY = _value;
    if (_value)
        m_vConstRotation.y = Get_Transform()->Get_EulerAngles().y;
}

_bool CRigidBody::IsConstRotationZ() const { return m_bConstRotationZ; }
void CRigidBody::SetConstRotationZ(_bool _value)
{
    if (m_bConstRotationZ == _value)
        return;

    m_bConstRotationZ = _value;
    if (_value)
        m_vConstRotation.z = Get_Transform()->Get_EulerAngles().z;
}

void CRigidBody::Translate(const vector3& _deltaWorld)
{
    if (_deltaWorld.lengthSq() <= 1e-12f)
        return;

    // 바디가 없으면 Transform만 이동
    if (!m_bHasBody && !m_bHasSensorBody)
    {
        Get_Transform()->Add_Position(_deltaWorld);
        CacheLastSyncedTransform(Get_Transform()->Get_Position(), Get_Transform()->Get_LocalQuaternion());
        return;
    }

    // 최신 Transform 확보 (주의: TransformStatic이면 Update가 스킵될 수 있음)
    Get_Transform()->Update();

    // 시작 포즈(Transform 기준)
    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

    vector3 startPos(static_cast<_float>(pos.GetX()), static_cast<_float>(pos.GetY()), static_cast<_float>(pos.GetZ()));
    quaternion startRot(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());

    // ---------------------------------------------
    // 1) ShapeCast로 이동 델타를 "충돌 전까지" 클램프(+슬라이드)
    // ---------------------------------------------
    vector3 desiredDelta = _deltaWorld;
    vector3 clampedDelta = desiredDelta;

    // 캐스팅에 사용할 shape / self body 선택
    const BodyID selfBodyId = m_bHasBody ? m_iBodyID : m_iSensorBodyID;
    const Shape* castShape = m_bHasBody ? m_pCompoundShape : m_pSensorCompoundShape;

    // physics 초기화/shape 유효할 때만 시도
    const bool canCast =
        CPhysics::GetInstance().IsInitialized() &&
        castShape != nullptr &&
        desiredDelta.lengthSq() > 1e-12f;

    // 스윕 실패 시를 대비한 플래그
    bool castSucceeded = false;

    // 마지막 히트 노멀(슬라이드/속도 처리에 활용)
    Vec3 lastHitNormal = Vec3::sZero();
    bool hadHit = false;

    if (canCast)
    {
        // 튜닝 파라미터
        const float skin = 0.01f;   // 벽에 살짝 띄워두는 여유 (단위/스케일 맞춰 조절)
        const int   iters = 3;      // 슬라이드 반복 횟수 (2~4 권장)

        auto& ps = GetPS();
        const auto& npq = ps.GetNarrowPhaseQuery();

        RVec3 curPos(pos.GetX(), pos.GetY(), pos.GetZ());
        Quat  curRot = rot;

        Vec3 remaining(desiredDelta.x, desiredDelta.y, desiredDelta.z);
        Vec3 moved = Vec3::sZero();

        for (int i = 0; i < iters && remaining.LengthSq() > 1e-12f; ++i)
        {
            // base offset은 start 근처로 두는 게 정밀도에 유리
            const RVec3 baseOffset = curPos;

            // 시작 변환
            const RMat44 startTM = RMat44::sRotationTranslation(curRot, curPos);

            // ShapeCast 구성 (MeshShape는 캐스트 불가일 수 있음)
            // 스케일은 1로 두되, 엔진에서 shape에 스케일을 baked 했다면 유지 가능
            const RShapeCast shapeCast = RShapeCast::sFromWorldTransform(
                castShape,
                Vec3::sReplicate(1.0f),
                startTM,
                remaining
            );

            ShapeCastSettings scs;

            ClosestHitCollisionCollector<CastShapeCollector> collector;
            IgnoreSingleBodyFilter bodyFilter(selfBodyId);

            npq.CastShape(shapeCast, scs, baseOffset, collector, {}, {}, bodyFilter);

            if (!collector.HadHit())
            {
                moved += remaining;
                curPos += remaining;
                castSucceeded = true;
                break;
            }

            // 히트 발생
            castSucceeded = true;
            hadHit = true;

            const auto& hit = collector.mHit;

            // fraction (0..1)
            float f = hit.mFraction;

            // skin을 반영해서 너무 딱 붙지 않게
            const float len = max(remaining.Length(), 1e-6f);
            const float skinFrac = skin / len;
            f = JPH::Clamp(f - skinFrac, 0.0f, 1.0f);

            Vec3 step = remaining * f;
            moved += step;
            curPos += step;

            lastHitNormal = -hit.mPenetrationAxis.Normalized();

            Vec3 rest = remaining - step;
            const float into = rest.Dot(lastHitNormal);
            if (into < 0.0f)
                rest -= lastHitNormal * into;

            remaining = rest;
        }

        clampedDelta = vector3(moved.GetX(), moved.GetY(), moved.GetZ());
    }

    if (!castSucceeded)
        clampedDelta = desiredDelta;

    if (clampedDelta.lengthSq() <= 1e-12f)
    {
        if (!m_bKinematic && m_bHasBody)
        {
            Vec3 v = GetBI().GetLinearVelocity(m_iBodyID);

            if (hadHit && lastHitNormal.LengthSq() > 1e-12f)
            {
                float into = v.Dot(lastHitNormal);
                if (into < 0.0f) v -= lastHitNormal * into;
            }
            GetBI().SetLinearAndAngularVelocity(m_iBodyID, v, GetBI().GetAngularVelocity(m_iBodyID));
        }
        return;
    }

    vector3 targetPos = startPos + clampedDelta;
    quaternion targetRot = startRot;

    if (m_bConstPositionX) m_vConstPosition.x = targetPos.x;
    if (m_bConstPositionY) m_vConstPosition.y = targetPos.y;
    if (m_bConstPositionZ) m_vConstPosition.z = targetPos.z;

    vector3 targetEuler = targetRot.to_euler();
    if (m_bConstRotationX) m_vConstRotation.x = targetEuler.x;
    if (m_bConstRotationY) m_vConstRotation.y = targetEuler.y;
    if (m_bConstRotationZ) m_vConstRotation.z = targetEuler.z;

    ApplyAxisConstraints(targetPos, targetRot);

    const float fixedDt = max(CPhysics::GetInstance().GetFixedDeltaTime(), 0.0001f);

    vector3 appliedDelta = targetPos - startPos;
    Vec3 desiredVel(
        static_cast<float>(appliedDelta.x / fixedDt),
        static_cast<float>(appliedDelta.y / fixedDt),
        static_cast<float>(appliedDelta.z / fixedDt)
    );

    if (hadHit && lastHitNormal.LengthSq() > 1e-12f)
    {
        const float into = desiredVel.Dot(lastHitNormal);
        if (into < 0.0f)
            desiredVel -= lastHitNormal * into;
    }

    Get_Transform()->Set_Position(targetPos);
    Get_Transform()->Set_Quaternion(targetRot);

    if (m_bHasBody)
    {
        const Vec3 ang = m_bKinematic ? Vec3::sZero() : GetBI().GetAngularVelocity(m_iBodyID);
        GetBI().SetLinearAndAngularVelocity(m_iBodyID, desiredVel, ang);
    }

    if (m_bHasSensorBody)
    {
        GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, desiredVel, Vec3::sZero());
    }

    CacheLastSyncedTransform(targetPos, targetRot);
}

void CRigidBody::Rotate(const vector3& _deltaEuler)
{
    if (_deltaEuler.lengthSq() <= 0.f)
        return;

    Get_Transform()->Update();

    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

    const quaternion currentRot(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
    const quaternion deltaRot = _deltaEuler.to_quaternion();

    XMVECTOR qCurrent = XMLoadFloat4(reinterpret_cast<const _float4*>(&currentRot));
    XMVECTOR qDelta = XMLoadFloat4(reinterpret_cast<const _float4*>(&deltaRot));
    XMVECTOR qTarget = XMQuaternionMultiply(qDelta, qCurrent);
    qTarget = XMQuaternionNormalize(qTarget);

    quaternion targetRot;
    XMStoreFloat4(reinterpret_cast<_float4*>(&targetRot), qTarget);
    vector3 targetPos(static_cast<_float>(pos.GetX()), static_cast<_float>(pos.GetY()), static_cast<_float>(pos.GetZ()));

    if (m_bConstPositionX)
        m_vConstPosition.x = targetPos.x;
    if (m_bConstPositionY)
        m_vConstPosition.y = targetPos.y;
    if (m_bConstPositionZ)
        m_vConstPosition.z = targetPos.z;

    vector3 targetEuler = targetRot.to_euler();
    if (m_bConstRotationX)
        m_vConstRotation.x = targetEuler.x;
    if (m_bConstRotationY)
        m_vConstRotation.y = targetEuler.y;
    if (m_bConstRotationZ)
        m_vConstRotation.z = targetEuler.z;

    ApplyAxisConstraints(targetPos, targetRot);

    Get_Transform()->Set_Position(targetPos);
    Get_Transform()->Set_Quaternion(targetRot);

    const RVec3 joltTargetPos(targetPos.x, targetPos.y, targetPos.z);
    const Quat joltTargetRot(targetRot.x, targetRot.y, targetRot.z, targetRot.w);

    if (m_bHasBody)
    {
        const Vec3 linearVel = GetBI().GetLinearVelocity(m_iBodyID);
        const Vec3 angularVel = GetBI().GetAngularVelocity(m_iBodyID);
        GetBI().SetPositionAndRotation(m_iBodyID, joltTargetPos, joltTargetRot, EActivation::Activate);
        GetBI().SetLinearAndAngularVelocity(m_iBodyID, linearVel, angularVel);
    }

    if (m_bHasSensorBody)
    {
        const Vec3 linearVel = GetBI().GetLinearVelocity(m_iSensorBodyID);
        const Vec3 angularVel = GetBI().GetAngularVelocity(m_iSensorBodyID);
        GetBI().SetPositionAndRotation(m_iSensorBodyID, joltTargetPos, joltTargetRot, EActivation::Activate);
        GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, linearVel, angularVel);
    }

    CacheLastSyncedTransform(targetPos, targetRot);
}

void CRigidBody::ApplyAxisConstraints(vector3& _pos, quaternion& _rot)
{
    vector3 euler = _rot.to_euler();

    if (m_bConstPositionX)
        _pos.x = m_vConstPosition.x;
    else
        m_vConstPosition.x = _pos.x;

    if (m_bConstPositionY)
        _pos.y = m_vConstPosition.y;
    else
        m_vConstPosition.y = _pos.y;

    if (m_bConstPositionZ)
        _pos.z = m_vConstPosition.z;
    else
        m_vConstPosition.z = _pos.z;

    if (m_bConstRotationX)
        euler.x = m_vConstRotation.x;
    else
        m_vConstRotation.x = euler.x;

    if (m_bConstRotationY)
        euler.y = m_vConstRotation.y;
    else
        m_vConstRotation.y = euler.y;

    if (m_bConstRotationZ)
        euler.z = m_vConstRotation.z;
    else
        m_vConstRotation.z = euler.z;

    _rot = euler.to_quaternion();
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
    CacheLastSyncedTransform(vector3(pos.GetX(), pos.GetY(), pos.GetZ()), quaternion(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()));

    if (bodyCompound != nullptr)
    {
        m_pCompoundShape = bodyCompound.GetPtr();
        m_pCompoundShape->AddRef();

        const EMotionType motion = m_bKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
        const ObjectLayer layer = Layers::MOVING;

        BodyCreationSettings settings(m_pCompoundShape, pos, rot, motion, layer);

        if (!m_bKinematic)
        {
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = m_fMass;

            const _float innerRadius = m_pCompoundShape ? m_pCompoundShape->GetInnerRadius() : 0.f;
            settings.mMotionQuality = innerRadius > 0.f ? EMotionQuality::LinearCast : EMotionQuality::Discrete;

            if (m_bConstPositionX && m_bConstPositionY && m_bConstPositionZ)
                settings.mAllowSleeping = false;
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
    m_bHasLastSyncedTransform = false;

    if (!CPhysics::GetInstance().IsInitialized())
    {
        m_iBodyID = BodyID();
        m_bHasBody = false;
        m_iSensorBodyID = BodyID();
        m_bHasSensorBody = false;
    }
    else
    {
        if (m_bHasBody)
        {
            CPhysics::GetInstance().RemoveContactPairs(m_iBodyID);
            GetBI().SetUserData(m_iBodyID, 0);
            GetBI().RemoveBody(m_iBodyID);
            GetBI().DestroyBody(m_iBodyID);
            m_iBodyID = BodyID();
            m_bHasBody = false;
        }

        if (m_bHasSensorBody)
        {
            CPhysics::GetInstance().RemoveContactPairs(m_iSensorBodyID);
            GetBI().SetUserData(m_iSensorBodyID, 0);
            GetBI().RemoveBody(m_iSensorBodyID);
            GetBI().DestroyBody(m_iSensorBodyID);
            m_iSensorBodyID = BodyID();
            m_bHasSensorBody = false;
        }
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
    if (!m_bHasBody && !m_bHasSensorBody)
        return;

    Get_Transform()->Update();

    Vec3 pos;
    Quat rot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);
    CacheLastSyncedTransform(vector3(pos.GetX(), pos.GetY(), pos.GetZ()), quaternion(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()));

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

    Get_Transform()->Update();

    const BodyID sourceBodyId = m_bHasBody ? m_iBodyID : m_iSensorBodyID;

    Vec3 tfPos;
    Quat tfRot;
    DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), tfPos, tfRot);

    const RVec3 joltPos = GetBI().GetPosition(sourceBodyId);
    const Quat joltRot = GetBI().GetRotation(sourceBodyId);

    const vector3 currentTfPos(tfPos.GetX(), tfPos.GetY(), tfPos.GetZ());
    const quaternion currentTfRot(tfRot.GetX(), tfRot.GetY(), tfRot.GetZ(), tfRot.GetW());

    const vector3 syncRefPos = m_bHasLastSyncedTransform ? m_vLastSyncedPosition : Get_Transform()->Get_PrevPosition();
    const quaternion syncRefRot = m_bHasLastSyncedTransform ? m_vLastSyncedRotation : Get_Transform()->Get_PrevQuaternion();

    const _float dpx = syncRefPos.x - tfPos.GetX();
    const _float dpy = syncRefPos.y - tfPos.GetY();
    const _float dpz = syncRefPos.z - tfPos.GetZ();
    const _float editedPosDeltaSq = dpx * dpx + dpy * dpy + dpz * dpz;

    const _float editedRotDotRaw = syncRefRot.x * tfRot.GetX() + syncRefRot.y * tfRot.GetY() + syncRefRot.z * tfRot.GetZ() + syncRefRot.w * tfRot.GetW();
    const _float editedRotDotAbs = fabsf(editedRotDotRaw);

    const _float physDx = static_cast<_float>(joltPos.GetX() - tfPos.GetX());
    const _float physDy = static_cast<_float>(joltPos.GetY() - tfPos.GetY());
    const _float physDz = static_cast<_float>(joltPos.GetZ() - tfPos.GetZ());
    const _float diffFromPhysicsPosSq = physDx * physDx + physDy * physDy + physDz * physDz;

    const _float diffFromPhysicsRotDotRaw =
        static_cast<_float>(joltRot.GetX() * tfRot.GetX() + joltRot.GetY() * tfRot.GetY() + joltRot.GetZ() * tfRot.GetZ() + joltRot.GetW() * tfRot.GetW());
    const _float diffFromPhysicsRotDotAbs = fabsf(diffFromPhysicsRotDotRaw);

    const _bool sceneTransformEdited = editedPosDeltaSq > 1e-8f || editedRotDotAbs < 0.9999999f;
    const _bool sceneTransformDiffersFromPhysics = diffFromPhysicsPosSq > 1e-8f || diffFromPhysicsRotDotAbs < 0.9999999f;
    const _bool transformChangedOutsidePhysics = sceneTransformEdited && sceneTransformDiffersFromPhysics;

    _bool forceTransformOverride = transformChangedOutsidePhysics;
#ifndef _CLIENT_BUILD
    CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject();
    if (selected != nullptr && selected == m_pGameObject)
        forceTransformOverride = forceTransformOverride || ImGuizmo::IsUsing();
#endif

    const _bool transformOverridden = forceTransformOverride;

    if (transformOverridden)
    {
        const RVec3 targetPos(tfPos.GetX(), tfPos.GetY(), tfPos.GetZ());
        const Quat targetRot = tfRot;

        vector3 editedPos(static_cast<_float>(tfPos.GetX()), static_cast<_float>(tfPos.GetY()), static_cast<_float>(tfPos.GetZ()));
        vector3 editedEuler = quaternion(tfRot.GetX(), tfRot.GetY(), tfRot.GetZ(), tfRot.GetW()).to_euler();

        if (m_bConstPositionX)
            m_vConstPosition.x = editedPos.x;
        if (m_bConstPositionY)
            m_vConstPosition.y = editedPos.y;
        if (m_bConstPositionZ)
            m_vConstPosition.z = editedPos.z;

        if (m_bConstRotationX)
            m_vConstRotation.x = editedEuler.x;
        if (m_bConstRotationY)
            m_vConstRotation.y = editedEuler.y;
        if (m_bConstRotationZ)
            m_vConstRotation.z = editedEuler.z;

        const _bool isGizmoEditing =
#ifndef _CLIENT_BUILD
            ImGuizmo::IsUsing();
#else
            false;
#endif

        const _float fixedDt = max(CPhysics::GetInstance().GetFixedDeltaTime(), 0.0001f);
        const Vec3 targetLinearVelocity(
            static_cast<float>((targetPos.GetX() - joltPos.GetX()) / fixedDt),
            static_cast<float>((targetPos.GetY() - joltPos.GetY()) / fixedDt),
            static_cast<float>((targetPos.GetZ() - joltPos.GetZ()) / fixedDt));

        if (m_bHasBody)
        {
            GetBI().SetPositionAndRotation(m_iBodyID, targetPos, targetRot, EActivation::Activate);
            if (isGizmoEditing)
                GetBI().SetLinearAndAngularVelocity(m_iBodyID, Vec3::sZero(), Vec3::sZero());
            else
                GetBI().SetLinearAndAngularVelocity(m_iBodyID, targetLinearVelocity, Vec3::sZero());
        }

        if (m_bHasSensorBody)
        {
            GetBI().SetPositionAndRotation(m_iSensorBodyID, targetPos, targetRot, EActivation::Activate);
            if (isGizmoEditing)
                GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, Vec3::sZero(), Vec3::sZero());
            else
                GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, targetLinearVelocity, Vec3::sZero());
        }

        CacheLastSyncedTransform(currentTfPos, currentTfRot);

        return;
    }

    vector3 pos = vector3(static_cast<_float>(joltPos.GetX()), static_cast<_float>(joltPos.GetY()), static_cast<_float>(joltPos.GetZ()));
    quaternion rot = quaternion(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());

    ApplyAxisConstraints(pos, rot);

    const _bool lockAllPosition = m_bConstPositionX && m_bConstPositionY && m_bConstPositionZ;
    const _bool lockAllRotation = m_bConstRotationX && m_bConstRotationY && m_bConstRotationZ;
    const EActivation activation = lockAllPosition ? EActivation::Activate : EActivation::DontActivate;

    Get_Transform()->Set_Position(pos);
    Get_Transform()->Set_Quaternion(rot);

    if (m_bHasBody)
        GetBI().SetPositionAndRotation(m_iBodyID, RVec3(pos.x, pos.y, pos.z), Quat(rot.x, rot.y, rot.z, rot.w), activation);

    if (m_bHasSensorBody)
        GetBI().SetPositionAndRotation(m_iSensorBodyID, RVec3(pos.x, pos.y, pos.z), Quat(rot.x, rot.y, rot.z, rot.w), activation);

    if (lockAllPosition || lockAllRotation)
    {
        if (m_bHasBody)
        {
            const Vec3 linearVel = lockAllPosition ? Vec3::sZero() : GetBI().GetLinearVelocity(m_iBodyID);
            const Vec3 angularVel = lockAllRotation ? Vec3::sZero() : GetBI().GetAngularVelocity(m_iBodyID);
            GetBI().SetLinearAndAngularVelocity(m_iBodyID, linearVel, angularVel);
        }

        if (m_bHasSensorBody)
        {
            const Vec3 linearVel = lockAllPosition ? Vec3::sZero() : GetBI().GetLinearVelocity(m_iSensorBodyID);
            const Vec3 angularVel = lockAllRotation ? Vec3::sZero() : GetBI().GetAngularVelocity(m_iSensorBodyID);
            GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, linearVel, angularVel);
        }
    }

    if (m_bHasBody && m_bHasSensorBody)
    {
        GetBI().SetPositionAndRotation(m_iSensorBodyID, RVec3(pos.x, pos.y, pos.z), Quat(rot.x, rot.y, rot.z, rot.w), EActivation::DontActivate);
        GetBI().SetLinearAndAngularVelocity(m_iSensorBodyID, GetBI().GetLinearVelocity(m_iBodyID), GetBI().GetAngularVelocity(m_iBodyID));
    }

    CacheLastSyncedTransform(pos, rot);
}

void CRigidBody::CacheLastSyncedTransform(const vector3& _pos, const quaternion& _rot)
{
    m_vLastSyncedPosition = _pos;
    m_vLastSyncedRotation = _rot;
    m_bHasLastSyncedTransform = true;
}

vector3 CRigidBody::ComputeClampedDelta_ByShapeCast(const vector3& _desiredDeltaWorld)
{
    if (!m_pCompoundShape || _desiredDeltaWorld.lengthSq() < 1e-8f)
        return _desiredDeltaWorld;

    const BodyID selfId = m_bHasBody ? m_iBodyID : m_iSensorBodyID;
    const _float skin = 0.01f; 
    const _int maxIter = 3;    

    auto& ps = GetPS();
    const auto& npq = ps.GetNarrowPhaseQuery(); // 엔진 구조에 맞춰 접근

    RVec3 curPos = GetBI().GetPosition(selfId);
    Quat  curRot = GetBI().GetRotation(selfId);

    Vec3 remaining(_desiredDeltaWorld.x, _desiredDeltaWorld.y, _desiredDeltaWorld.z);
    Vec3 moved = Vec3::sZero();

    for (_int i = 0; i < maxIter && remaining.LengthSq() > 1e-8f; ++i)
    {
        RVec3 baseOffset = curPos;

        RMat44 start = RMat44::sRotationTranslation(curRot, curPos);

        RShapeCast shape_cast = RShapeCast::sFromWorldTransform(m_pCompoundShape, Vec3::sReplicate(1.0f), start, remaining);

        ShapeCastSettings scs;

        ClosestHitCollisionCollector<CastShapeCollector> collector;
        IgnoreSingleBodyFilter body_filter(selfId);

        npq.CastShape(shape_cast, scs, baseOffset, collector, {}, {}, body_filter);

        if (!collector.HadHit())
        {
            moved += remaining;
            curPos += remaining;
            break;
        }

        const auto& hit = collector.mHit;
        float f = hit.mFraction;

        f = JPH::Clamp(f - (skin / max(remaining.Length(), 1e-6f)), 0.0f, 1.0f);

        Vec3 step = remaining * f;
        moved += step;
        curPos += step;

        Vec3 n = -hit.mPenetrationAxis.Normalized();

        Vec3 rest = remaining - step;
        float into = rest.Dot(n);
        if (into < 0.0f)
            rest -= n * into;

        remaining = rest;
    }

    return vector3(moved.GetX(), moved.GetY(), moved.GetZ());
}
