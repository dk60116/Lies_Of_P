#include "epch.h"
#include "BehaviourTree.h"
#include "BTNode.h"
#include "GameObject.h"
#include "Transform.h"

CBehaviourTree::CBehaviourTree()
	: m_pRoot(nullptr)
	, m_tContext({})
	, m_eLastState(BTState::Failure)
{
	m_strName = L"BehaviourTree";
}

CBehaviourTree::~CBehaviourTree()
{
}

CBehaviourTree* CBehaviourTree::Create()
{
	return new CBehaviourTree();
}

CComponent* CBehaviourTree::Clone() const
{
	CBehaviourTree* clone = new CBehaviourTree();
	clone->m_tContext = m_tContext;
	clone->m_tContext.owner = nullptr;
	clone->m_tContext.distanceToTarget = 0.f;
	clone->m_eLastState = m_eLastState;

	return clone;
}

HRESULT CBehaviourTree::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	RefreshContext();
	return S_OK;
}

void CBehaviourTree::Awake()
{
	RefreshContext();
}

void CBehaviourTree::OnEnable()
{
	if (m_pRoot)
		m_pRoot->Reset();
}

void CBehaviourTree::OnDisable()
{
	if (m_pRoot)
		m_pRoot->Reset();
}

void CBehaviourTree::Update()
{
	if (!m_pRoot)
		return;

	RefreshContext();
	m_eLastState = m_pRoot->Update(m_tContext);
}

void CBehaviourTree::Render_Gizmo()
{
}

void CBehaviourTree::OnDestroy()
{
	ClearTree();
	m_tContext.owner = nullptr;
	m_tContext.target = nullptr;
	m_tContext.hasTarget = false;
	m_tContext.canSeeTarget = false;
	m_tContext.distanceToTarget = 0.f;
}

void CBehaviourTree::SetRoot(CBTNode* _root)
{
	if (m_pRoot == _root)
		return;

	if (_root)
		_root->AddRef();

	Safe_Release(m_pRoot);
	m_pRoot = _root;

	if (m_pRoot)
		m_pRoot->Reset();
}

CBTNode* CBehaviourTree::GetRoot()
{
	return m_pRoot;
}

const CBTNode* CBehaviourTree::GetRoot() const
{
	return m_pRoot;
}

AIContext& CBehaviourTree::GetContext()
{
	return m_tContext;
}

const AIContext& CBehaviourTree::GetContext() const
{
	return m_tContext;
}

void CBehaviourTree::SetTarget(CGameObject* _target)
{
	m_tContext.target = _target;
	RefreshContext();
}

void CBehaviourTree::ClearTarget()
{
	m_tContext.target = nullptr;
	RefreshContext();
}

BTState CBehaviourTree::GetLastState() const
{
	return m_eLastState;
}

void CBehaviourTree::ClearTree()
{
	Safe_Release(m_pRoot);
}

void CBehaviourTree::RefreshContext()
{
	m_tContext.owner = m_pGameObject;
	m_tContext.hasTarget = (m_tContext.target != nullptr);
	m_tContext.distanceToTarget = 0.f;

	if (!m_tContext.target)
		m_tContext.canSeeTarget = false;

	if (!m_tContext.owner || !m_tContext.target)
		return;

	CTransform* ownerTransform = m_tContext.owner->Get_Transform();
	CTransform* targetTransform = m_tContext.target->Get_Transform();
	if (!ownerTransform || !targetTransform)
		return;

	m_tContext.distanceToTarget = vector3::Distance(ownerTransform->Get_Position(), targetTransform->Get_Position());
}
