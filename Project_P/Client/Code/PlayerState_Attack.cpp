#include "cpch.h"
#include "PlayerState_Attack.h"
#include "PlayerController.h"
#include "Player.h"

CPlayerState_Attack::CPlayerState_Attack()
    : m_bCanContinue(false)
    , m_bPressedContinue(false)
    , m_iCrtCombo(0)
    , m_bUnderTerm(false)
    , m_bUnderLimit(false)
    , m_iComboTerm()
    , m_iComboLimit()
    , m_iComboTerm_S()
    , m_iComboLimit_S()
    , m_bStrong(false)
    , m_bThrust(false)
{
}

CPlayerState_Attack::~CPlayerState_Attack()
{
}

void CPlayerState_Attack::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
    __super::Initialize(_ctx, _type);

    m_iComboTerm[0] = 6;
    m_iComboTerm[1] = 10;
    m_iComboTerm[2] = 12;
    m_iComboTerm[3] = 18;

    m_iComboLimit[0] = 12;
    m_iComboLimit[1] = 15;
    m_iComboLimit[2] = 15;
    m_iComboLimit[3] = 23;

    m_iComboTerm_S[0] = 9;
    m_iComboTerm_S[1] = 22;
    m_iComboTerm_S[2] = 55;

    m_iComboLimit_S[0] = 16;
    m_iComboLimit_S[1] = 25;
    m_iComboLimit_S[2] = 62;

    const _float endRate = 0.78f;
    const _uint thrustEnableBoxFrame = 11;

    const auto registerActionTrigger = [this](CAnimationClip* clip, const _uint frame, const wstring& triggerName, const function<void()>& handler)
    {
        CAnimationClip::ActionTrigger at = { frame, triggerName };
        clip->Add_ActionTrigger(at);
        m_pCtx->Animator()->RegisterActionHandler(triggerName, [this, handler]()
            {
                if (!m_pCtx->IsActionActive(m_eStateType))
                    return;

                handler();
            });
    };

    const auto beginCombo = [this](const _bool canTurnOnStart, const function<void()>& onStart)
    {
        ++m_iCrtCombo;

        // 콤보 순환 시 리셋 (라이트 4콤보, 스트롱 3콤보)
        const _int maxCombo = m_bStrong ? 3 : 4;
        if (m_iCrtCombo > maxCombo)
            m_iCrtCombo = 1;

        if (onStart)
            onStart();

        m_pCtx->Get_Player()->SetWeaponKnockback(m_iCrtCombo <= 1);

        m_pCtx->Animator()->SetInt(L"attackCombo", m_iCrtCombo);
        m_pCtx->Animator()->SetBool(L"comboContinue", false);
        m_bCanContinue = true;
        m_bPressedContinue = false;
        m_bUnderTerm = true;
        m_bUnderLimit = true;

        if (canTurnOnStart)
            m_pCtx->SetCanTurn(true);
    };

    const auto registerComboWindow = [this, &registerActionTrigger]
    (
        CAnimationClip* clip,
        const _uint termFrame,
        const wstring& termTrigger,
        const _uint limitFrame,
        const wstring& limitTrigger)
    {
        registerActionTrigger(clip, termFrame, termTrigger, [this]()
            {
                m_bUnderTerm = false;

                if (m_bPressedContinue)
                    ContinueCombo();
            });

        registerActionTrigger(clip, limitFrame, limitTrigger, [this]()
            {
                m_pCtx->Get_Player()->DisableSwordCollider();
                m_bCanContinue = false;
                m_bUnderLimit = false;
            });
    };

    const auto registerComboClip = [this, endRate, &registerActionTrigger, &beginCombo, &registerComboWindow](
        const wstring& clipName,
        const wstring& startTrigger,
        const wstring& termTrigger,
        const wstring& limitTrigger,
        const wstring& endTrigger,
        const _uint termFrame,
        const _uint limitFrame,
        const _bool canTurnOnStart,
        const function<void()>& onStart) -> CAnimationClip*
    {
        CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(clipName);
        const _uint endFrame = clip->Get_NormalizedFrameIndex(endRate);

        registerActionTrigger(clip, 1, startTrigger, [beginCombo, canTurnOnStart, onStart]()
            {
                beginCombo(canTurnOnStart, onStart);
            });
        registerComboWindow(clip, termFrame, termTrigger, limitFrame, limitTrigger);
        registerActionTrigger(clip, endFrame, endTrigger, [this]()
            {
                Exit();
            });

        return clip;
    };

    for (_int i = 1; i <= 4; ++i)
    {
        const wstring attackIndex = to_wstring(i);

        CAnimationClip* clip = registerComboClip
        (
            L"Eve_Attack_Light_0" + attackIndex + L" (Animation Clip)",
            L"LightAttack0" + attackIndex + L"_Enter",
            L"LightAttack0" + attackIndex + L"_Term",
            L"LightAttack0" + attackIndex + L"_Limit",
            L"LightAttack0" + attackIndex + L"_Exit",
            m_iComboTerm[i - 1],
            m_iComboLimit[i - 1],
            true,
            nullptr
        );
    }

    for (_int i = 1; i <= 3; ++i)
    {
        const wstring attackIndex = to_wstring(i);

        CAnimationClip* clip = registerComboClip
        (
            L"Eve_Attack_Strong_0" + attackIndex + L" (Animation Clip)",
            L"StrongAttack_0" + attackIndex + L"_Enter",
            L"StrongAttack_0" + attackIndex + L"_Term",
            L"StrongAttack_0" + attackIndex + L"_Limit",
            L"StrongAttack_0" + attackIndex + L"_Exit",
            m_iComboTerm_S[i - 1],
            m_iComboLimit_S[i - 1],
            true,
            nullptr
        );
    }

    struct AttackSequenceDesc
    {
        const wchar_t* clipName;
        const wchar_t* triggerPrefix;
        _uint enableBoxFrame;
        _uint termFrame;
        _uint limitFrame;
        _bool canTurnOnStart;
    };

    const AttackSequenceDesc attackSequenceDescs[] =
    {
        { L"Eve_Attack_LS12 (Animation Clip)", L"Eve_Attack_LS12", 4, 20, 24, true },
        { L"Eve_Attack_SS23 (Animation Clip)", L"Eve_Attack_SS23", 4, 27, 32, false },
        { L"Eve_Attack_SS34 (Animation Clip)", L"Eve_Attack_SS34", 4, 16, 20, false },
        { L"Eve_Attack_SL23 (Animation Clip)", L"Eve_Attack_SL23", 4, 18, 24, false },
        { L"Eve_Attack_SL12 (Animation Clip)", L"Eve_Attack_SL12", 4, 15, 18, false },
        { L"Eve_Attack_LS23 (Animation Clip)", L"Eve_Attack_LS23", 4, 18, 21, false },
        { L"Eve_Attack_SL34 (Animation Clip)", L"Eve_Attack_SL34", 4, 20, 23, false },
        { L"Eve_Attack_LS45 (Animation Clip)", L"Eve_Attack_LS45", 4, 32, 35, false },
        { L"Eve_Attack_LLS23 (Animation Clip)", L"Eve_Attack_LLS23", 4, 21, 24, false },
        { L"Eve_Attack_LLLS34 (Animation Clip)", L"Eve_Attack_LLLS34", 4, 21, 24, false }
    };

    for (const AttackSequenceDesc& desc : attackSequenceDescs)
    {
        const wstring prefix = desc.triggerPrefix;

        CAnimationClip* clip = registerComboClip
        (
            desc.clipName,
            prefix + L"_Start",
            prefix + L"_Term",
            prefix + L"_Limit",
            prefix + L"_End",
            desc.termFrame,
            desc.limitFrame,
            desc.canTurnOnStart,
            nullptr
        );
    }

    CAnimationClip* thrustClip = registerComboClip
    (
        L"Eve_Attack_LLSS34 (Animation Clip)",
        L"Eve_Attack_Thrust_Start",
        L"Eve_Attack_Thrust_Term",
        L"Eve_Attack_Thrust_Limit",
        L"Eve_Attack_Thrust_End",
        21,
        24,
        false,
        [this]()
        {
            m_bThrust = true;
        }
    );

    registerActionTrigger(thrustClip, 11, L"Eve_Attack_Thrust_Stop", [this]()
        {
            m_bThrust = false;
            m_pCtx->StopMoveImmediate();
        });
}

void CPlayerState_Attack::Enter()
{
	__super::Enter();

    const _bool pendingStrongAttack = m_pCtx->ConsumePendingStrongAttack();
    const _bool isLeftClickEnter = m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack);
    const _bool isRightClickEnter = m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack_S);

    m_bStrong = pendingStrongAttack;

    if (isLeftClickEnter)
        m_bStrong = false;
    else if (pendingStrongAttack || (isRightClickEnter && !isLeftClickEnter))
        m_bStrong = true;

    m_pCtx->SetBattle(true);

    m_pCtx->SetCanMove(false);
    m_pCtx->SetCanTurn(false);

    m_pCtx->SetCanDashAttack(false);
    m_pCtx->SetCanJump(false);

    m_pCtx->Animator()->SetInt(L"attackCombo", 0);
    m_pCtx->SetAnimMoveSpeed(0.f);
    m_pCtx->Animator()->SetTrigger(L"attack");
    m_pCtx->Animator()->SetBool(L"isAttack", !m_bStrong);
    m_pCtx->Animator()->SetBool(L"isStrongAttack", m_bStrong);
    m_pCtx->Animator()->SetBool(L"comboContinue", false);

    m_iCrtCombo = 0;
    m_bCanContinue = true;
    m_bPressedContinue = false;
    m_bUnderTerm = true;
    m_bUnderLimit = true;
    m_bThrust = false;

    m_pCtx->Get_Player()->OnSwordAttackHandler();
    m_pCtx->Get_Player()->SetWeaponKnockback(true);

    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(true);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(true);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(true);

    m_pCtx->Get_Player()->GetRigidBody()->ResetVelocity();
}

void CPlayerState_Attack::Update()
{
    __super::Update();

    if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack))
        m_bStrong = false;
    if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack_S))
        m_bStrong = true;

    m_pCtx->Animator()->SetBool(L"isAttack", !m_bStrong);
    m_pCtx->Animator()->SetBool(L"isStrongAttack", m_bStrong);

    if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack) || m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack_S))
    {
        if (m_bCanContinue && m_bUnderTerm)
            m_bPressedContinue = true;
        else
            ContinueCombo();

        if (!m_bUnderLimit)
        {
            Enter();
            return;
        }
    }

    if (m_bThrust)
    {
        m_pCtx->AddPosition(m_pCtx->GetCharacterDir() * 10.f * DELTA_TIME);
    }

    if (m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move) && !m_bUnderLimit)
    {
        Exit();
    }
}

void CPlayerState_Attack::Exit()
{
    __super::Exit();

    m_pCtx->Animator()->SetBool(L"comboContinue", false);
    m_pCtx->Animator()->SetBool(L"isAttack", false);
    m_pCtx->Animator()->SetBool(L"isStrongAttack", false);

    m_pCtx->SetCanMove(true);
    m_pCtx->SetCanTurn(true);

    m_pCtx->SetCanDashAttack(true);
    m_pCtx->SetCanJump(true);

    m_pCtx->Get_Player()->DisableSwordCollider();

    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(false);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(false);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(false);

    m_pCtx->Get_Player()->SetAbleNavAgent(false);
}

void CPlayerState_Attack::ContinueCombo()
{
    if (m_bCanContinue)
    {
        m_pCtx->Animator()->SetBool(L"comboContinue", true);
    }
}



