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

void CPlayerState_Attack::Initialize(CPlayerControllerContext* _ctx)
{
    __super::Initialize(_ctx);

    CAnimationClip* ealClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Attack_Light_01 (Animation Clip)");

    {
        CAnimationClip::ActionTrigger at = { 0, L"LightAttack01_Enter" };
        ealClip->Add_ActionTrigger(at);
        m_pCtx->Animator()->RegisterActionHandler(L"LightAttack01_Enter", []() { CDebug::LogError("Enter01"); });
    }
}

void CPlayerState_Attack::Enter()
{
	__super::Enter();

    m_pCtx->SetAttackActive(true);

    m_pCtx->SetAnimMoveSpeed(0.f);
    m_pCtx->Animator()->SetTrigger(L"Attack");
    m_pCtx->Animator()->SetBool(L"IsAttack", true);
    m_pCtx->Animator()->SetBool(L"comboContinue", false);

    m_iCombo = 0;
    m_iPrevCombo = 0;
}

void CPlayerState_Attack::Update()
{
    __super::Update();

    const _float nt = m_pCtx->Animator()->GetNormalizedTime();

    if (m_iCombo != m_iPrevCombo)
        m_iPrevCombo = m_iCombo;

    for (_int i = 0; i <= 4; ++i)
    {
        if (m_iCombo == i)
        {
            if (nt < m_fComboLimit[i])
            {
                if (m_pCtx->IsLightAttackPressed())
                {
                    m_pCtx->Animator()->SetBool(L"comboContinue", true);
                    TurnPlayer();
                    ++m_iCombo;
                    m_fPassedTime = 0.f;
                }
            }
            else
            {
                if (m_pCtx->IsLightAttackPressed())
                    Enter();
            }

            if (nt > m_fEndTime[i])
                m_pCtx->SetAttackActive(false);
        }

        if (m_iCombo >= 4)
            m_pCtx->SetAttackActive(false);
    }
}

void CPlayerState_Attack::Exit()
{
    __super::Exit();

    m_pCtx->SetAttackActive(false);
    m_bQueuedNext = false;

    m_pCtx->Animator()->SetBool(L"comboContinue", false);
    m_pCtx->Animator()->SetBool(L"IsAttack", false);
}

void CPlayerState_Attack::TurnPlayer()
{
    m_pCtx->SetPlayerYaw(m_pCtx->GetCameraYaw());
}
