#include "epch.h"
#include "Collider.h"

using namespace JPH;

CCollider::CCollider()
	: m_pRigidBody(nullptr)
	, m_bIsTrigger(false)
	, m_bShapeDirty(true)
	, m_pShape(nullptr)
{
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
