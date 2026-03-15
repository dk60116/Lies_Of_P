#include "cpch.h"
#include "MonsterController.h"
#include "BehaviourTree.h"
#include "GameManager.h"
#include "Monster.h"
#include "Player.h"
#include "Transform.h"

CMonsterController::CMonsterController()
	: m_pMonster(nullptr)
	, m_pBT(nullptr)
	, m_eState(MonsterState::Patrol)
	, m_eDefaultState(MonsterState::Patrol)
	, m_bHasTarget(false)
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
	clone->m_eState = m_eState;
	clone->m_eDefaultState = m_eDefaultState;
	clone->m_bHasTarget = m_bHasTarget;

	return clone;
}

HRESULT CMonsterController::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CMonsterController::Awake()
{
	Set_State(m_eDefaultState);
}

void CMonsterController::Start()
{
}

void CMonsterController::Update()
{
	Refresh_Target();
	Set_State(Resolve_State());
}

void CMonsterController::LateUpdate()
{
}

void CMonsterController::OnDestroy()
{
	Safe_Release(m_pMonster);
	Safe_Release(m_pBT);
}

void CMonsterController::Bind(CMonster* _monster, CBehaviourTree* _bt)
{
	if (m_pMonster != _monster)
	{
		Safe_Release(m_pMonster);
		m_pMonster = _monster;

		if (m_pMonster)
			m_pMonster->AddRef();
	}

	if (m_pBT != _bt)
	{
		Safe_Release(m_pBT);
		m_pBT = _bt;

		if (m_pBT)
			m_pBT->AddRef();
	}

	if (m_pMonster)
		m_pMonster->Change_State(static_cast<_uint>(m_eState));
}

void CMonsterController::Set_State(const MonsterState _state)
{
	if (m_eState == _state)
		return;

	m_eState = _state;

	if (m_pMonster)
		m_pMonster->Change_State(static_cast<_uint>(m_eState));
}

void CMonsterController::Set_DefaultState(const MonsterState _state)
{
	m_eDefaultState = _state;

	if (!m_bHasTarget)
		Set_State(m_eDefaultState);
}

CMonster* CMonsterController::Get_Monster() const
{
	return m_pMonster;
}

CBehaviourTree* CMonsterController::Get_BT() const
{
	return m_pBT;
}

CMonsterController::MonsterState CMonsterController::Get_State() const
{
	return m_eState;
}

CMonsterController::MonsterState CMonsterController::Get_DefaultState() const
{
	return m_eDefaultState;
}

const _bool CMonsterController::Has_Target() const
{
	return m_bHasTarget;
}

void CMonsterController::Refresh_Target()
{
	if (!m_pMonster || !m_pBT)
		return;

	CPlayer* player = CGameManager::GetInstance().Get_Player();
	if (!player)
	{
		m_bHasTarget = false;
		m_pBT->ClearTarget();
		return;
	}

	CTransform* monsterTransform = m_pMonster->GetTransform();
	CTransform* playerTransform = player->GetTransform();
	if (!monsterTransform || !playerTransform)
	{
		m_bHasTarget = false;
		m_pBT->ClearTarget();
		return;
	}

	if (m_bHasTarget)
	{
		m_pBT->SetTarget(player->Get_GameObject());
		return;
	}

	const _float distance = vector3::Distance(monsterTransform->Get_Position(), playerTransform->Get_Position());
	const _float detectionRange = m_pMonster->GetStatus().detectionRange;

	if (distance > detectionRange)
		return;

	m_pBT->SetTarget(player->Get_GameObject());
	m_bHasTarget = true;
}

CMonsterController::MonsterState CMonsterController::Resolve_State() const
{
	if (!m_pMonster)
		return m_eDefaultState;

	if (m_pMonster->HasPendingHitReaction() || m_pMonster->IsHitReacting())
		return MonsterState::Hit;

	if (!m_bHasTarget || !m_pBT)
		return m_eDefaultState;

	CPlayer* player = CGameManager::GetInstance().Get_Player();
	if (!player)
		return m_eDefaultState;

	const AIContext& ctx = m_pBT->GetContext();
	const CMonster::MonsterStatus& status = m_pMonster->GetStatus();
	const _float stopBuffer = max(0.5f, m_pMonster->GetRadius());
	const _float battleEnterDistance = 2.f + m_pMonster->GetRadius() + player->GetRadius() + status.attackRange + stopBuffer;
	const _float battleExitDistance = battleEnterDistance + stopBuffer * 2.f;

	if (m_eState == MonsterState::Battle)
	{
		if (ctx.distanceToTarget <= battleExitDistance)
			return MonsterState::Battle;
	}
	else
	{
		if (ctx.distanceToTarget <= battleEnterDistance)
			return MonsterState::Battle;
	}

	return MonsterState::Chase;
}
