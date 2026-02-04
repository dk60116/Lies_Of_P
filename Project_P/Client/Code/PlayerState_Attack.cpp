#include "cpch.h"
#include "PlayerState_Attack.h"

CPlayerState_Attack::CPlayerState_Attack()
    : m_iCombo(0)
    , m_bQueuedNext(false)
    , m_fComboTerm()
{
    m_fComboTerm[0] = 0.34f;
    m_fComboTerm[1] = 0.37f;
    m_fComboTerm[2] = 0.46f;
    m_fComboTerm[3] = 0.58f;
}

CPlayerState_Attack::~CPlayerState_Attack()
{
}

void CPlayerState_Attack::Enter(CPlayerControllerContext& _ctx)
{
	__super::Enter(_ctx);

    _ctx.SetAttackActive(true);

    _ctx.SetAnimSpeed(0.f);
    _ctx.Animator()->SetTrigger(L"Attack");

    m_iCombo = 0;
}

void CPlayerState_Attack::Update(CPlayerControllerContext& _ctx)
{
    __super::Update(_ctx);

    _ctx.Animator()->SetBool(L"comboContinue", false);

    for (_int i = 0; i <= 4; ++i)
    {
        if (m_iCombo == i)
        {
            if (m_fPassedTime >= m_fComboTerm[i] && _ctx.IsLightAttackPressed())
            {
                _ctx.Animator()->SetBool(L"comboContinue", true);
                ++m_iCombo;
                m_fPassedTime = 0.f;
            }
        }
    }

    if (m_fPassedTime >= 1.f)
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

    m_fPassedTime = 0.f;
    m_bQueuedNext = false;
}
