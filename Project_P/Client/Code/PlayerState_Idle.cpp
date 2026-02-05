#include "cpch.h"
#include "PlayerState_Idle.h"

void CPlayerState_Idle::Enter()
{
	__super::Enter();
	m_pCtx->SetAnimMoveSpeed(0.f);
}

void CPlayerState_Idle::Update()
{
	__super::Update();

	m_pCtx->TickMove();
}
