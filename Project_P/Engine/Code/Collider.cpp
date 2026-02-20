#include "epch.h"
#include "Collider.h"
#include "RigidBody.h"

using namespace JPH;

CCollider::CCollider()
	: m_pRigidBody(nullptr)
	, m_bIsTrigger(false)
	, m_vCachedScale(vector3::one())
	, m_bShapeDirty(true)
	, m_pShape(nullptr)
	, m_iContactCount(0)
{
	m_strName = L"Collider";
}

CCollider::~CCollider()
{
}

HRESULT CCollider::Initialize()
{
	if (auto rig = m_pGameObject->GetComponent<CRigidBody>())
		rig->AddCollider(this);

	return S_OK;
}

void CCollider::Awake()
{
	m_vCachedScale = m_pGameObject->Get_Transform()->Get_LocalScale();
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

		if (m_pRigidBody)
			m_pRigidBody->MarkBodyDirty();
	}
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
}

void CCollider::OnDestroy()
{
	if (m_pRigidBody)
		m_pRigidBody->RemvoeCollier(this);

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

	if (m_pRigidBody)
		m_pRigidBody->MarkBodyDirty();
}

void CCollider::SetCenter(const vector3& center)
{
	m_vCenter = center;
	m_bShapeDirty = true;

	if (m_pRigidBody)
		m_pRigidBody->MarkBodyDirty();
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
