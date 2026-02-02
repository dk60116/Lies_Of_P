#include "cpch.h"
#include "PlayerState_Locomotion.h"

CPlayerState_Locomotion::CPlayerState_Locomotion()
	: m_pChild(nullptr)
	, m_pMove(nullptr)
	, m_pIdle(nullptr)
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

void CPlayerState_Locomotion::Enter(CPlayerControllerContext& _ctx)
{
	__super::Enter(_ctx);

	m_pChild = m_pIdle;
	if (m_pChild) 
		m_pChild->Enter(_ctx);
}

void CPlayerState_Locomotion::Update(CPlayerControllerContext& _ctx)
{
	__super::Update(_ctx);

	_ctx.TickAttackBuffer();

	if (_ctx.IsAttackActive() || _ctx.HasAttackBuffered())
		TransitionTo(_ctx, m_pAttack);
	else
		TransitionTo(_ctx, _ctx.IsMovePressed() ? m_pMove : m_pIdle);

	if (m_pChild)
		m_pChild->Update(_ctx);

	_ctx.TickTurn(_ctx.PlayerStat().turnSpeed);
	_ctx.TickMove();
}

void CPlayerState_Locomotion::Exit(CPlayerControllerContext& _ctx)
{
	__super::Exit(_ctx);

	if (m_pChild)
		m_pChild->Exit(_ctx);
	m_pChild = nullptr;
}

void CPlayerState_Locomotion::TransitionTo(CPlayerControllerContext& _ctx, CPlayerState* _next)
{
	if (!_next || _next == m_pChild)
		return;

	if (m_pChild)
		m_pChild->Exit(_ctx);
	m_pChild = _next;
	m_pChild->Enter(_ctx);
}
