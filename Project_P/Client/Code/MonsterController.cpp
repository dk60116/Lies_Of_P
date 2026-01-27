#include "cpch.h"
#include "MonsterController.h"
#include "Monster.h"
#include "Behaviour_Idle.h"
#include "Behaviour_Patrole.h"
#include "Behaviour_Tracking.h"
#include "Behaviour_Combat.h"
#include "Behaviour_Find.h"

CMonsterController::CMonsterController()
	: m_pMonster(nullptr)
	, m_eCrtState(MonsterState::Idle)
	, m_mBehaviourList({})
	, m_pCrtBehaviour(nullptr)
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
	CBehaviour_Idle* idle = new CBehaviour_Idle();
	idle->Initialize(m_pMonster);
	idle->AddRef();
	m_mBehaviourList.emplace(Idle, idle);

	CBehaviour_Patrole* patrole = new CBehaviour_Patrole();
	patrole->Initialize(m_pMonster);
	patrole->AddRef();
	m_mBehaviourList.emplace(Patrole, patrole);

	CBehaviour_Tracking* tracking = new CBehaviour_Tracking();
	tracking->Initialize(m_pMonster);
	tracking->AddRef();
	m_mBehaviourList.emplace(Tracking, tracking);

	CBehaviour_Combat* combat = new CBehaviour_Combat();
	combat->Initialize(m_pMonster);
	combat->AddRef();
	m_mBehaviourList.emplace(Combat, combat);

	CBehaviour_Find* find = new CBehaviour_Find();
	find->Initialize(m_pMonster);
	find->AddRef();
	m_mBehaviourList.emplace(Find, find);

	if (m_pMonster)
	{
		//ChangeState(Idle);
	}
}

void CMonsterController::Start()
{
}

void CMonsterController::Update()
{
	if (m_pMonster)
		UpdateControleState();

	if (m_pCrtBehaviour)
		m_pCrtBehaviour->During();
}

void CMonsterController::OnDestroy()
{
	Safe_Release(m_pMonster);

	for (TRAVERSAL_ITER(m_mBehaviourList, it))
		Safe_Release((*it).second);

	m_mBehaviourList.clear();
}

CMonster* CMonsterController::Get_Monster()
{
	return m_pMonster;
}

void CMonsterController::Set_Monster(CMonster* _monster)
{
	if (_monster == m_pMonster)
		return;

	Safe_Release(m_pMonster);

	m_pMonster = _monster;

	if (m_pMonster)
		m_pMonster->AddRef();
}

void CMonsterController::UpdateControleState()
{
	CTransform* transform = m_pMonster->Get_Transform();
	const vector3 myPosition = transform->Get_Position();
	const vector3 myAngle = transform->Get_EulerAngles();

	CPlayer* player = CGameManager::GetInstance().Get_Player();
	CTransform* playerTf = player->Get_Transform();
	const vector3 playerPos = playerTf->Get_Position();
	const vector3 playerAnge = playerTf->Get_EulerAngles();
}

void CMonsterController::ChangeState(MonsterState _state)
{
	if (m_pCrtBehaviour)
		m_pCrtBehaviour->Exit();
	m_pCrtBehaviour = m_mBehaviourList[_state];
	m_pCrtBehaviour->Enter();
}
