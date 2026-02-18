#include "epch.h"
#include "Collider.h"

using namespace JPH;

CCollider::CCollider()
	: m_pRigidBody(nullptr)
	, m_bIsTrigger(false)
	, m_bShapeDirty(true)
	, m_pShape(nullptr)
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
	if (auto rig = m_pGameObject->GetComponent<CRigidBody>())
		rig->RemvoeCollier(this);

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
}

void CCollider::SetCenter(const vector3& center)
{
	m_vCenter = center;
	m_bShapeDirty = true;
}

Shape* CCollider::GetShadpe()
{
	return m_pShape;
}
