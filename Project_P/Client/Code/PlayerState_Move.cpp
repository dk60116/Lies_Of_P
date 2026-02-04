#include "cpch.h"
#include "PlayerState_Move.h"

void CPlayerState_Move::Initialize(CPlayerControllerContext& _ctx)
{
}

void CPlayerState_Move::Enter(CPlayerControllerContext& _ctx)
{
    _ctx.SetAnimMoveSpeed(0.2f);
}

void CPlayerState_Move::Update(CPlayerControllerContext& _ctx)
{
    __super::Update(_ctx);

    const vector3& dir = _ctx.GetMoveWorldDir();
    const _float speed = _ctx.PlayerStat().moveSpeed;
    _ctx.AddPosition(dir * speed * DELTA_TIME);

    _ctx.BeginTurnTo(_ctx.GetDesiredYawDeg());

    if (m_fPassedTime >= 0.5f)
        _ctx.SetAnimMoveSpeed(1.f);
}
