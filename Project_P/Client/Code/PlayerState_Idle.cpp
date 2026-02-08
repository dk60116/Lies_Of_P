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

	if (m_pCtx->IsBattle())
	{
		if (m_fPassedTime >= 4.f)
			m_pCtx->SetBattle(false);
	}

}
