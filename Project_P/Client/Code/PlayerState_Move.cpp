#include "cpch.h"
#include "PlayerState_Move.h"

void CPlayerState_Move::Enter(CPlayerControllerContext& _ctx)
{
    _ctx.SetAnimSpeed(1.f);
}

void CPlayerState_Move::Update(CPlayerControllerContext& _ctx)
{
    const vector3& dir = _ctx.GetMoveWorldDir();
    const _float speed = _ctx.PlayerStat().moveSpeed;
    _ctx.AddPosition(dir * speed * DELTA_TIME);

    _ctx.BeginTurnTo(_ctx.GetDesiredYawDeg());

    _ctx.SetAnimSpeed(1.f);
}
