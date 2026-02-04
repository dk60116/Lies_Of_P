#include "cpch.h"
#include "PlayerState_Attack.h"

CPlayerState_Attack::CPlayerState_Attack()
    : m_iCombo(0)
    , m_bQueuedNext(false)
    , m_fComboTerm()
    , m_fEndTime()
{
    m_fComboTerm[0] = 0.32f;
    m_fComboTerm[1] = 0.35f;
    m_fComboTerm[2] = 0.45f;

    m_fEndTime[0] = 0.8f;
    m_fEndTime[1] = 0.8f;
    m_fEndTime[2] = 1.f;
    m_fEndTime[3] = 1.5f;
}

CPlayerState_Attack::~CPlayerState_Attack()
{
}

void CPlayerState_Attack::Enter(CPlayerControllerContext& _ctx)
{
	__super::Enter(_ctx);

    _ctx.SetAttackActive(true);

    _ctx.SetAnimMoveSpeed(0.f);
    _ctx.Animator()->SetTrigger(L"Attack");
    _ctx.Animator()->SetBool(L"IsAttack", true);

    m_bDash = false;

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
                TurnPlayer(_ctx);
                ++m_iCombo;
                m_fPassedTime = 0.f;
                m_bDash = true;
            }

            if (m_fPassedTime >= m_fEndTime[i])
                _ctx.SetAttackActive(false);
        }

        if (m_iCombo >= 4)
            _ctx.SetAttackActive(false);
    }
}

void CPlayerState_Attack::Exit(CPlayerControllerContext& _ctx)
{
    __super::Exit(_ctx);

    _ctx.SetAttackActive(false);
    m_bQueuedNext = false;

    _ctx.Animator()->SetBool(L"comboContinue", false);
    _ctx.Animator()->SetBool(L"IsAttack", false);
}

void CPlayerState_Attack::PlayStep(CPlayerControllerContext& _ctx, _int _idx)
{
    auto anim = _ctx.PlayerStat();
    auto a = _ctx.Animator();

    m_fPassedTime = 0.f;
    m_bQueuedNext = false;
}

void CPlayerState_Attack::TurnPlayer(CPlayerControllerContext& _ctx)
{
    _ctx.SetPlayerYaw(_ctx.GetCameraYaw());
}
