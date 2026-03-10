#include "epch.h"
#include "BehaviourTree.h"

CBehaviourTree::CBehaviourTree()
{
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

	return clone;
}

HRESULT CBehaviourTree::Initialize()
{
	return S_OK;
}

void CBehaviourTree::Awake()
{
}

void CBehaviourTree::OnEnable()
{
}

void CBehaviourTree::OnDisable()
{
}

void CBehaviourTree::Update()
{
}

void CBehaviourTree::Render_Gizmo()
{
}

void CBehaviourTree::OnDestroy()
{
}
