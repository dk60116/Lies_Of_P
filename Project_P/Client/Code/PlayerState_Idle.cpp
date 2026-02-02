#include "cpch.h"
#include "PlayerState_Idle.h"

void CPlayerState_Idle::Enter(CPlayerControllerContext& _ctx)
{
	_ctx.SetAnimSpeed(0.f);
}

void CPlayerState_Idle::Update(CPlayerControllerContext& _ctx)
{
}
