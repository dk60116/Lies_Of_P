#include "epch.h"
#include "Collider.h"
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

using namespace JPH;

CCollider::CCollider()
	: m_pRigidBody(nullptr)
	, m_bIsTrigger(false)
	, m_bShapeDirty(true)
	, m_pShape(nullptr)
	, m_bIsColliding(false)
{
	m_strName = L"Collider";
}

CCollider::~CCollider()
{
}

HRESULT CCollider::Initialize()
{
	return S_OK;
}

void CCollider::Awake()
{
}

void CCollider::Update()
{
}

void CCollider::ReleaseShape()
{
	if (m_pShape)
	{
		m_pShape->Release();
		m_pShape = nullptr;
	}
}

void CCollider::OnDestroy()
{
	ReleaseShape();
}

const _bool CCollider::IsTrigger() const
{
	return m_bIsTrigger;
}

const vector3& CCollider::GetCenter() const
{
	return m_vCenter;
}


const _bool CCollider::IsColliding() const
{
	return m_bIsColliding;
}

void CCollider::SetTrigger(const _bool isTrigger)
{
	m_bIsTrigger = isTrigger;
	m_bShapeDirty = true;
}

void CCollider::SetCenter(const vector3& center)
{
	m_vCenter = center;
	m_bShapeDirty = true;
}

void CCollider::SetColliding(const _bool isColliding)
{
	m_bIsColliding = isColliding;
}

_bool CCollider::Intersects(CCollider* other)
{
	if (!other)
		return false;

	BuildShapeIfNeeded();
	other->BuildShapeIfNeeded();

	if (!m_pShape || !other->m_pShape)
		return false;

	const _matrix worldA = Get_Transform()->Get_WorldMatrix();
	const _matrix worldB = other->Get_Transform()->Get_WorldMatrix();

	_float3 posA = {};
	_float3 posB = {};
	_float4 rotA = {};
	_float4 rotB = {};

	XMStoreFloat3(&posA, worldA.r[3]);
	XMStoreFloat3(&posB, worldB.r[3]);
	XMStoreFloat4(&rotA, XMQuaternionRotationMatrix(worldA));
	XMStoreFloat4(&rotB, XMQuaternionRotationMatrix(worldB));

	JPH::RVec3 jpPosA(posA.x, posA.y, posA.z);
	JPH::RVec3 jpPosB(posB.x, posB.y, posB.z);
	JPH::Quat jpRotA(rotA.x, rotA.y, rotA.z, rotA.w);
	JPH::Quat jpRotB(rotB.x, rotB.y, rotB.z, rotB.w);

	JPH::CollideShapeSettings settings;
	JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
	JPH::ShapeFilter shapeFilter;

	JPH::CollisionDispatch::sCollideShapeVsShape(
		m_pShape,
		other->m_pShape,
		JPH::Vec3::sReplicate(1.f),
		JPH::Vec3::sReplicate(1.f),
		JPH::Mat44::sRotationTranslation(jpRotA, jpPosA),
		JPH::Mat44::sRotationTranslation(jpRotB, jpPosB),
		JPH::SubShapeIDCreator(),
		JPH::SubShapeIDCreator(),
		settings,
		collector,
		shapeFilter);

	return collector.HadHit();
}
