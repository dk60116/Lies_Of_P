#include "cpch.h"
#include "PlayerState_Locomotion.h"

CPlayerState_Locomotion::CPlayerState_Locomotion()
	: m_pIdle(nullptr)
	, m_pMove(nullptr)
	, m_pChild(nullptr)
{
}

CPlayerState_Locomotion::~CPlayerState_Locomotion()
{
}

void CPlayerState_Locomotion::SetChildren(CPlayerState* _idle, CPlayerState* _move)
{
	m_pIdle = _idle;
	m_pMove = _move;
}

void CPlayerState_Locomotion::Enter(CPlayerControllerContext& _ctx)
{
	m_pChild = m_pIdle;
	if (m_pChild) 
		m_pChild->Enter(_ctx);
}

void CPlayerState_Locomotion::Update(CPlayerControllerContext& _ctx)
{
    TransitionTo(_ctx, _ctx.IsMovePressed() ? m_pMove : m_pIdle);

    if (m_pChild) 
		m_pChild->Update(_ctx);

    _ctx.TickTurn(_ctx.PlayerStat().turnSpeed, 1.f);
}

void CPlayerState_Locomotion::Exit(CPlayerControllerContext& _ctx)
{
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
