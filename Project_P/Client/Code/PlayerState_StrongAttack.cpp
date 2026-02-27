#include "cpch.h"
#include "PlayerState_StrongAttack.h"


CPlayerState_StrongAttack::CPlayerState_StrongAttack()
    : m_bCanContinue(false)
    , m_bPressedContinue(false)
    , m_iCrtCombo(0)
    , m_bUnderTerm(false)
    , m_bUnderLimit(false)
    , m_iComboTerm()
    , m_iComboLimit()
    , m_iTurnLock()
    , m_bLastContinue(false)
{
}

CPlayerState_StrongAttack::~CPlayerState_StrongAttack()
{

}

void CPlayerState_StrongAttack::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
    __super::Initialize(_ctx, _type);

    m_iComboTerm[0] = 6;
    m_iComboTerm[1] = 8;
    m_iComboTerm[2] = 12;

    m_iComboLimit[0] = 12;
    m_iComboLimit[1] = 15;
    m_iComboLimit[2] = 18;

    m_iTurnLock[0] = 0;
    m_iTurnLock[1] = 10;
    m_iTurnLock[2] = 10;

    for (_int i = 1; i <= 3; ++i)
    {
        CAnimationClip* ealClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Attack_Strong_0" + to_wstring(i) + L" (Animation Clip)");

        const wstring clipName = ealClip->Get_ResourceName();
        const _uint frameCount = ealClip->Get_FrameCount();
        const _uint endFrame = ealClip->Get_NormalizedFrameIndex(0.78f);
        const _uint termFrame = m_iComboTerm[i - 1];
        const _uint limitFrame = m_iComboLimit[i - 1];
        const _uint turnLockFrame = m_iTurnLock[i - 1];

        {
            CAnimationClip::ActionTrigger at = { 1, L"StrongAttack_0" + to_wstring(i) + L"_Enter" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"StrongAttack_0" + to_wstring(i) + L"_Enter", [this]()
                {
                    ++m_iCrtCombo;
                    m_pCtx->Animator()->SetInt(L"AttackCombo", m_iCrtCombo);
                    m_pCtx->Animator()->SetBool(L"comboContinue", false);
                    m_bCanContinue = true;
                    m_bPressedContinue = false;
                    m_bUnderTerm = true;
                    m_bUnderLimit = true;
                    m_pCtx->SetCanTurn(true);
                });
        }

        {
            CAnimationClip::ActionTrigger at = { endFrame, L"StrongAttack_0" + to_wstring(i) + L"_Exit" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"StrongAttack_0" + to_wstring(i) + L"_Exit", [this]()
                {
                    m_pCtx->SetActionActive(CPlayerController::PlayerState::Attack, false);
                    m_pCtx->SetCanTurn(true);
                });
        }

        // Continue Term
        {
            CAnimationClip::ActionTrigger at = { termFrame, L"StrongAttack_0" + to_wstring(i) + L"_Term" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"StrongAttack_0" + to_wstring(i) + L"_Term", [this]()
                {
                    m_bUnderTerm = false;

                    if (m_bPressedContinue)
                        ContinueCombo();

                    m_pCtx->SetCanTurn(true);
                });
        }

        // Continue Limit
        {
            CAnimationClip::ActionTrigger at = { limitFrame, L"StrongAttack_0" + to_wstring(i) + L"_Limit" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"StrongAttack_0" + to_wstring(i) + L"_Limit", [this]()
                {
                    m_bCanContinue = false;
                    m_bUnderLimit = false;
                });
        }

        // Turn
        {
            CAnimationClip::ActionTrigger at = { turnLockFrame, L"StrongAttack_0" + to_wstring(i) + L"_Turn" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"StrongAttack_0" + to_wstring(i) + L"_Turn", [this]()
                {
                    m_pCtx->SetCanTurn(false);
                });
        }
    }
}

void CPlayerState_StrongAttack::Enter()
{
    __super::Enter();

    m_pCtx->SetBattle(true);

    m_pCtx->SetCanMove(false);
    m_pCtx->SetCanJump(false);

    m_pCtx->Animator()->SetInt(L"AttackCombo", 0);
    m_pCtx->SetAnimMoveSpeed(0.f);
    m_pCtx->Animator()->SetTrigger(L"Attack");
    m_pCtx->Animator()->SetBool(L"isStrongAttack", true);
    m_pCtx->Animator()->SetBool(L"comboContinue", false);

    m_iCrtCombo = 0;
    m_bCanContinue = true;
    m_bPressedContinue = false;
    m_bUnderTerm = true;
    m_bUnderLimit = true;
    m_bLastContinue = false;
}

void CPlayerState_StrongAttack::Update()
{
}

void CPlayerState_StrongAttack::Exit()
{
}

void CPlayerState_StrongAttack::ContinueCombo()
{
}