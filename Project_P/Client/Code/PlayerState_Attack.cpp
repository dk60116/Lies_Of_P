#include "cpch.h"
#include "PlayerState_Attack.h"
#include "PlayerController.h"
#include "PlayerState_StrongAttack.h"

CPlayerState_Attack::CPlayerState_Attack()
    : m_bCanContinue(false)
    , m_bPressedContinue(false)
    , m_iCrtCombo(0)
    , m_bUnderTerm(false)
    , m_bUnderLimit(false)
    , m_iComboTerm()
    , m_iComboLimit()
    , m_bStrong(false)
    , m_bLastContinue(false)
{
}

CPlayerState_Attack::~CPlayerState_Attack()
{
}

void CPlayerState_Attack::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
    __super::Initialize(_ctx, _type);

    m_iComboTerm[0] = 6;
    m_iComboTerm[1] = 8;
    m_iComboTerm[2] = 12;

    m_iComboLimit[0] = 12;
    m_iComboLimit[1] = 15;
    m_iComboLimit[2] = 18;
    m_iComboLimit[3] = 21;

    for (_int i = 1; i <= 4; ++i)
    {
        CAnimationClip* ealClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Attack_Light_0" + to_wstring(i) + L" (Animation Clip)");

        const wstring clipName = ealClip->Get_ResourceName();
        const _uint frameCount = ealClip->Get_FrameCount();
        const _uint endFrame = ealClip->Get_NormalizedFrameIndex(0.78f);
        const _uint termFrame = m_iComboTerm[i - 1];
        const _uint limitFrame = m_iComboLimit[i - 1];

        {
            CAnimationClip::ActionTrigger at = { 1, L"LightAttack0" + to_wstring(i) + L"_Enter" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"LightAttack0" + to_wstring(i) + L"_Enter", [this, i]()
                {
                    ++m_iCrtCombo;
                    m_pCtx->Animator()->SetInt(L"AttackCombo", m_iCrtCombo);
                    m_pCtx->Animator()->SetBool(L"comboContinue", false);
                    m_bCanContinue = true;
                    m_bPressedContinue = false;
                    m_bUnderTerm = true;
                    m_bUnderLimit = true;

                    if (i > 1)
                        m_pCtx->SetCanTurn(true);
                });
        }

        {
            CAnimationClip::ActionTrigger at = { endFrame, L"LightAttack0" + to_wstring(i) + L"_Exit" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"LightAttack0" + to_wstring(i) + L"_Exit", [this]()
                {
                    m_pCtx->SetActionActive(CPlayerController::PlayerState::Attack, false);
                });
        }
        
        // Continue Term
        {
            CAnimationClip::ActionTrigger at = { termFrame, L"LightAttack0" + to_wstring(i) + L"_Term" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"LightAttack0" + to_wstring(i) + L"_Term", [this]()
                {
                    m_bUnderTerm = false;

                    if (m_bPressedContinue)
                        ContinueCombo();
                });
        }

        // Continue Limit
        {
            CAnimationClip::ActionTrigger at = { limitFrame, L"LightAttack0" + to_wstring(i) + L"_Limit" };
            ealClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"LightAttack0" + to_wstring(i) + L"_Limit", [this]()
                {
                    m_bCanContinue = false;
                    m_bUnderLimit = false;
                });
        }
    }
}

void CPlayerState_Attack::Enter()
{
	__super::Enter();

    m_pCtx->SetBattle(true);

    m_pCtx->SetCanMove(false);
    m_pCtx->SetCanTurn(false);
    m_pCtx->SetCanJump(false);

    m_pCtx->Animator()->SetInt(L"AttackCombo", 0);
    m_pCtx->SetAnimMoveSpeed(0.f);
    m_pCtx->Animator()->SetTrigger(L"Attack");
    m_pCtx->Animator()->SetBool(L"isAttack", true);
    m_pCtx->Animator()->SetBool(L"comboContinue", false);

    m_iCrtCombo = 0;
    m_bCanContinue = true;
    m_bPressedContinue = false;
    m_bUnderTerm = true;
    m_bUnderLimit = true;
    m_bLastContinue = false;
    m_bStrong = false;
}

void CPlayerState_Attack::Update()
{
    __super::Update();

    if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack))
    {
        if (m_bUnderTerm)
            m_bPressedContinue = true;
        else
            ContinueCombo();
    }

    if (m_iCrtCombo >= 4)
    {
        if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack) && m_bUnderLimit)
        {
            m_bLastContinue = true;
            return;
        }

        if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack) && !m_bUnderLimit)
        {
            Enter();
            return;
        }
    }

    if (m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move) && !m_bUnderLimit)
        m_pCtx->SetActionActive(CPlayerController::PlayerState::Attack, false);

    if (m_pCtx->IsBigTurn())
        Exit();
}

void CPlayerState_Attack::Exit()
{
    __super::Exit();

    m_pCtx->Animator()->SetBool(L"comboContinue", false);
    m_pCtx->Animator()->SetBool(L"isAttack", false);
    m_pCtx->Animator()->SetBool(L"isStrongAttack", false);

    m_pCtx->SetCanMove(true);
    m_pCtx->SetCanTurn(true);
    m_pCtx->SetCanJump(true);
}

void CPlayerState_Attack::ContinueCombo()
{
    if (m_bLastContinue)
    {
        Enter();
        return;
    }

    if (m_bCanContinue)
    {
        m_pCtx->Animator()->SetBool(L"isAttack", !m_bStrong);
        m_pCtx->Animator()->SetBool(L"isStrongAttack", m_bStrong);
        m_pCtx->Animator()->SetBool(L"comboContinue", true);
    }
    else
        Enter();
}
