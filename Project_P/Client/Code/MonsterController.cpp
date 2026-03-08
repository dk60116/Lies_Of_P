#include "cpch.h"
#include "MonsterController.h"

CMonsterController::CMonsterController()
{
}

CMonsterController::~CMonsterController()
{
}

CMonsterController* CMonsterController::Create()
{
	return new CMonsterController();
}

CComponent* CMonsterController::Clone() const
{
	CMonsterController* clone = new CMonsterController();

	return clone;
}

HRESULT CMonsterController::Initialize()
{
	return S_OK;
}

void CMonsterController::Awake()
{
}

void CMonsterController::Start()
{
}

void CMonsterController::Update()
{
}

void CMonsterController::LateUpdate()
{
}

void CMonsterController::OnDestroy()
{
}
