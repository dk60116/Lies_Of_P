#include "epch.h"
#include "Collider.h"
#include "RigidBody.h"
#include "Physics.h"

using namespace JPH;

namespace
{
	constexpr uint64 g_iColliderUserDataFlag = 1ull;

	inline uint64 EncodeColliderUserData(CCollider* _collider)
	{
		return static_cast<uint64>(reinterpret_cast<uintptr_t>(_collider)) | g_iColliderUserDataFlag;
	}


	inline void DecomposeWorldMatrix(const _matrix& _matrix, Vec3& _outPos, Quat& _outRot)
	{
		XMVECTOR s, r, t;
		XMMatrixDecompose(&s, &r, &t, _matrix);

		XMFLOAT3 p;
		XMStoreFloat3(&p, t);
		_outPos = Vec3(p.x, p.y, p.z);

		XMFLOAT4 q;
		XMStoreFloat4(&q, r);
		_outRot = Quat(q.x, q.y, q.z, q.w);
	}
}

CCollider::CCollider()
	: m_pRigidBody(nullptr)
	, m_bIsTrigger(false)
	, m_vCachedScale(vector3::one())
	, m_bShapeDirty(true)
	, m_pShape(nullptr)
	, m_iContactCount(0)
	, m_iStandaloneBodyID(BodyID())
	, m_bHasStandaloneBody(false)
	, m_bStandaloneBodyDirty(true)
	, m_vCachedWorldPosition(vector3::zero())
	, m_vCachedWorldRotation(quaternion::identity())
{
	m_strName = L"Collider";
}

CCollider::~CCollider()
{
}

HRESULT CCollider::Initialize()
{
	CRigidBody* rig = m_pGameObject->GetComponent<CRigidBody>();
	if (rig)
		rig->AddCollider(this);

	return S_OK;
}

void CCollider::Awake()
{
	m_vCachedScale = m_pGameObject->Get_Transform()->Get_LocalScale();
	m_vCachedWorldPosition = m_pGameObject->Get_Transform()->Get_Position();
	m_vCachedWorldRotation = m_pGameObject->Get_Transform()->Get_Quaternion();
	RefreshStandaloneBody();
}

void CCollider::Update()
{
	const vector3 scale = m_pGameObject->Get_Transform()->Get_LocalScale();
	const _float dx = fabsf(scale.x - m_vCachedScale.x);
	const _float dy = fabsf(scale.y - m_vCachedScale.y);
	const _float dz = fabsf(scale.z - m_vCachedScale.z);

	if (dx > 0.0001f || dy > 0.0001f || dz > 0.0001f)
	{
		m_vCachedScale = scale;
		m_bShapeDirty = true;
		m_bStandaloneBodyDirty = true;

		if (m_pRigidBody)
			m_pRigidBody->MarkBodyDirty();
	}

	const vector3 pos = m_pGameObject->Get_Transform()->Get_Position();
	const quaternion rot = m_pGameObject->Get_Transform()->Get_Quaternion();

	const _float dpx = fabsf(pos.x - m_vCachedWorldPosition.x);
	const _float dpy = fabsf(pos.y - m_vCachedWorldPosition.y);
	const _float dpz = fabsf(pos.z - m_vCachedWorldPosition.z);
	const _float dot = fabsf(rot.x * m_vCachedWorldRotation.x + rot.y * m_vCachedWorldRotation.y + rot.z * m_vCachedWorldRotation.z + rot.w * m_vCachedWorldRotation.w);
	const _bool moved = dpx > 0.0001f || dpy > 0.0001f || dpz > 0.0001f || dot < 0.9999f;

	if (moved)
	{
		m_vCachedWorldPosition = pos;
		m_vCachedWorldRotation = rot;
		m_bStandaloneBodyDirty = true;
	}

	if (!m_pRigidBody)
		RefreshStandaloneBody();
}

void CCollider::ReleaseShape()
{
	if (m_pShape)
	{
		m_pShape->Release();
		m_pShape = nullptr;
	}
}

void CCollider::SetRigidBody(CRigidBody* rigidBody)
{
	m_pRigidBody = rigidBody;

	if (m_pRigidBody)
	{
		DestroyStandaloneBody();
		return;
	}

	m_bStandaloneBodyDirty = true;
	RefreshStandaloneBody();
}

void CCollider::OnDestroy()
{
	if (m_pRigidBody)
		m_pRigidBody->RemvoeCollier(this);

	DestroyStandaloneBody();
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

void CCollider::SetTrigger(const _bool isTrigger)
{
	m_bIsTrigger = isTrigger;
	m_bShapeDirty = true;
	m_bStandaloneBodyDirty = true;

	if (m_pRigidBody)
		m_pRigidBody->MarkBodyDirty();
	else
		RefreshStandaloneBody();
}

void CCollider::SetCenter(const vector3& center)
{
	m_vCenter = center;
	m_bShapeDirty = true;
	m_bStandaloneBodyDirty = true;

	if (m_pRigidBody)
		m_pRigidBody->MarkBodyDirty();
	else
		RefreshStandaloneBody();
}

const Shape* CCollider::GetShape()
{
	BuildShapeIfNeeded();
	return m_pShape;
}

const _bool CCollider::IsContacting() const
{
	return m_iContactCount > 0;
}

void CCollider::BeginContact()
{
	++m_iContactCount;
}

void CCollider::EndContact()
{
	if (m_iContactCount > 0)
		--m_iContactCount;
}

void CCollider::RefreshStandaloneBody()
{
	if (m_pRigidBody)
		return;

	if (m_bStandaloneBodyDirty)
	{
		DestroyStandaloneBody();
		CreateStandaloneBody();
		m_bStandaloneBodyDirty = !m_bHasStandaloneBody;
		return;
	}

	if (!m_bHasStandaloneBody)
		CreateStandaloneBody();

	SyncStandaloneBodyTransform();
}

void CCollider::CreateStandaloneBody()
{
	if (m_pRigidBody || m_bHasStandaloneBody)
		return;

	const Shape* shape = GetShape();

	if (!shape)
		return;

	Vec3 pos;
	Quat rot;
	DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

	const ObjectLayer layer = m_bIsTrigger ? Layers::SENSOR : Layers::NON_MOVING;
	BodyCreationSettings settings(shape, pos, rot, EMotionType::Static, layer);
	settings.mIsSensor = m_bIsTrigger;

	BodyInterface& bodyInterface = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	Body* body = bodyInterface.CreateBody(settings);
	if (!body)
		return;

	m_iStandaloneBodyID = body->GetID();
	m_bHasStandaloneBody = true;

	bodyInterface.SetUserData(m_iStandaloneBodyID, EncodeColliderUserData(this));
	bodyInterface.AddBody(m_iStandaloneBodyID, EActivation::DontActivate);
}

void CCollider::DestroyStandaloneBody()
{
	if (!m_bHasStandaloneBody || !m_pRigidBody)
		return;

	BodyInterface& bodyInterface = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	CPhysics::GetInstance().RemoveContactPairs(m_iStandaloneBodyID);
	bodyInterface.RemoveBody(m_iStandaloneBodyID);
	bodyInterface.DestroyBody(m_iStandaloneBodyID);

	m_iStandaloneBodyID = BodyID();
	m_bHasStandaloneBody = false;
	m_iContactCount = 0;
}

void CCollider::SyncStandaloneBodyTransform()
{
	if (!m_bHasStandaloneBody ||!m_pRigidBody)
		return;

	Vec3 pos;
	Quat rot;
	DecomposeWorldMatrix(Get_Transform()->Get_WorldMatrix(), pos, rot);

	BodyInterface& bodyInterface = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	bodyInterface.SetPositionAndRotation(m_iStandaloneBodyID, RVec3(pos), rot, EActivation::Activate);
}
