#include "cpch.h"
#include "PlayerState_Attack.h"

void CPlayerState_Attack::Enter(CPlayerControllerContext& _ctx)
{
	__super::Enter(_ctx);

    _ctx.Animator()->SetTrigger(L"Attack");
}

void CPlayerState_Attack::Update(CPlayerControllerContext& _ctx)
{
    __super::Update(_ctx);

    CDebug::LogError(m_fPassedTime);

    if (m_fPassedTime >= 5.f)
        _ctx.SetAttackActive(false);
}

void CPlayerState_Attack::Exit(CPlayerControllerContext& _ctx)
{
    __super::Exit(_ctx);

    _ctx.SetAttackActive(false);
    m_bQueuedNext = false;
}

void CPlayerState_Attack::PlayStep(CPlayerControllerContext& _ctx, _int _idx)
{
    auto anim = _ctx.PlayerStat();
    auto a = _ctx.Animator();

    m_t = 0.f;
    m_bQueuedNext = false;
}
