#include "cpch.h"
#include "PlayerState_Locomotion.h"

CPlayerState_Locomotion::CPlayerState_Locomotion()
	: m_pChild(nullptr)
	, m_pMove(nullptr)
	, m_pIdle(nullptr)
	, m_pAttack(nullptr)
{
}

CPlayerState_Locomotion::~CPlayerState_Locomotion()
{
}

void CPlayerState_Locomotion::SetChildren(CPlayerState* _idle, CPlayerState* _move, CPlayerState* _attack)
{
	m_pIdle = _idle;
	m_pMove = _move;
	m_pAttack = _attack;
}

void CPlayerState_Locomotion::Enter()
{
	__super::Enter();

	m_pChild = m_pIdle;

	if (m_pChild) 
		m_pChild->Enter();
}

void CPlayerState_Locomotion::Update()
{
	__super::Update();

	m_pCtx->TickAttackBuffer();

	if (m_pCtx->IsAttackActive() || m_pCtx->HasAttackBuffered())
		TransitionTo(m_pAttack);
	else
		TransitionTo(m_pCtx->IsMovePressed() ? m_pMove : m_pIdle);

	if (m_pChild)
		m_pChild->Update();

	m_pCtx->TickTurn(m_pCtx->PlayerStatus().turnSpeed);
	m_pCtx->TickMove();
}

void CPlayerState_Locomotion::Exit()
{
	__super::Exit();

	if (m_pChild)
		m_pChild->Exit();

	m_pChild = nullptr;
}

void CPlayerState_Locomotion::TransitionTo(CPlayerState* _next)
{
	if (!_next || _next == m_pChild)
		return;

	if (m_pChild)
		m_pChild->Exit();
	m_pChild = _next;
	m_pChild->Enter();
}
