#include "epch.h"
#include "NaviMeshAgent.h"

CNaviMeshAgent::CNaviMeshAgent()
{
}

CNaviMeshAgent::~CNaviMeshAgent()
{
}

CNaviMeshAgent* CNaviMeshAgent::Create()
{
	return new CNaviMeshAgent();
}

CComponent* CNaviMeshAgent::Clone() const
{
	CNaviMeshAgent* clone = new CNaviMeshAgent();

	return clone;
}

HRESULT CNaviMeshAgent::Initialize()
{
	return S_OK;
}

void CNaviMeshAgent::Awake()
{
}

void CNaviMeshAgent::OnEnable()
{
}

void CNaviMeshAgent::OnDisable()
{
}

void CNaviMeshAgent::Update()
{
}

void CNaviMeshAgent::OnDestroy()
{
}
