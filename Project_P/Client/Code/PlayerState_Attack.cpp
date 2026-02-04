#include "cpch.h"
#include "PlayerState_Attack.h"

CPlayerState_Attack::CPlayerState_Attack()
    : m_iCombo(0)
    , m_iPrevCombo(0)
    , m_bQueuedNext(false)
    , m_fComboTerm()
    , m_fComboLimit()
    , m_fEndTime()
{
    m_fComboTerm[0] = 0.1f;
    m_fComboTerm[1] = 0.1f;
    m_fComboTerm[2] = 0.1f;

    m_fComboLimit[0] = 0.15f;
    m_fComboLimit[1] = 0.15f;
    m_fComboLimit[2] = 0.15f;

    m_fEndTime[0] = 0.8f;
    m_fEndTime[1] = 0.8f;
    m_fEndTime[2] = 0.8f;
    m_fEndTime[3] = 0.8f;
}

CPlayerState_Attack::~CPlayerState_Attack()
{
}

void CPlayerState_Attack::Initialize(CPlayerControllerContext& _ctx)
{
    CAnimationClip* ealClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Attack_Light_01 (Animation Clip)");
    CAnimationClip::ActionTrigger at = { 0, L"LightAttack01_Enter" };
    ealClip->Add_ActionTrigger(at);

    _ctx.Animator()->RegisterActionHandler(L"LightAttack01_Enter", []() { CDebug::LogError("Enter01"); });
}

void CPlayerState_Attack::Enter(CPlayerControllerContext& _ctx)
{
	__super::Enter(_ctx);

    _ctx.SetAttackActive(true);

    _ctx.SetAnimMoveSpeed(0.f);
    _ctx.Animator()->SetTrigger(L"Attack");
    _ctx.Animator()->SetBool(L"IsAttack", true);
    _ctx.Animator()->SetBool(L"comboContinue", false);

    m_iCombo = 0;
    m_iPrevCombo = 0;
}

void CPlayerState_Attack::Update(CPlayerControllerContext& _ctx)
{
    __super::Update(_ctx);

    const _float nt = _ctx.Animator()->GetNormalizedTime();

    if (m_iCombo != m_iPrevCombo)
        m_iPrevCombo = m_iCombo;

    for (_int i = 0; i <= 4; ++i)
    {
        if (m_iCombo == i)
        {
            if (nt < m_fComboLimit[i])
            {
                if (_ctx.IsLightAttackPressed())
                {
                    _ctx.Animator()->SetBool(L"comboContinue", true);
                    TurnPlayer(_ctx);
                    ++m_iCombo;
                    m_fPassedTime = 0.f;
                }
            }
            else
            {
                if (_ctx.IsLightAttackPressed())
                    Enter(_ctx);
            }

            if (nt > m_fEndTime[i])
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
