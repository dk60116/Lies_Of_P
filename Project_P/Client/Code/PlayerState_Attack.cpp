#include "cpch.h"
#include "PlayerState_Attack.h"
#include "PlayerController.h"
#include "Player.h"

namespace
{
    struct AttackClipDesc
    {
        const wchar_t* clipName;
        const wchar_t* triggerPrefix;
        _uint enableBoxFrame;
        _uint termFrame;
        _uint limitFrame;
        _bool canTurnOnStart;
        _float knockbackAmount;
    };

    constexpr AttackClipDesc kLightAttackDescs[] =
    {
        { L"Eve_Attack_Light_01 (Animation Clip)", L"LightAttack_01", 4, 8, 12, true, 2.f },
        { L"Eve_Attack_Light_02 (Animation Clip)", L"LightAttack_02", 4, 10, 15, true, 1.5f },
        { L"Eve_Attack_Light_03 (Animation Clip)", L"LightAttack_03", 4, 12, 15, true, 1.5f },
        { L"Eve_Attack_Light_04 (Animation Clip)", L"LightAttack_04", 6, 18, 23, true, 4.f }
    };

    constexpr AttackClipDesc kStrongAttackDescs[] =
    {
        { L"Eve_Attack_Strong_01 (Animation Clip)", L"StrongAttack_01", 4, 9, 16, true, 1.f },
        { L"Eve_Attack_Strong_02 (Animation Clip)", L"StrongAttack_02", 4, 22, 25, true, 0.f },
        { L"Eve_Attack_Strong_03 (Animation Clip)", L"StrongAttack_03", 4, 55, 62, true, 1.f }
    };

    struct AttackSequenceDesc
    {
        const wchar_t* clipName;
        const wchar_t* triggerPrefix;
        _uint enableBoxFrame;
        _uint termFrame;
        _uint limitFrame;
        _bool canTurnOnStart;
        _float knockbackAmount;
    };

    constexpr AttackSequenceDesc kAttackSequenceDescs[] =
    {
        { L"Eve_Attack_LS12 (Animation Clip)", L"Eve_Attack_LS12", 4, 20, 24, true, 0.f },
        { L"Eve_Attack_SS23 (Animation Clip)", L"Eve_Attack_SS23", 4, 27, 32, false, 0.f },
        { L"Eve_Attack_SS34 (Animation Clip)", L"Eve_Attack_SS34", 4, 16, 20, false, 0.f },
        { L"Eve_Attack_SL23 (Animation Clip)", L"Eve_Attack_SL23", 4, 18, 24, false, 0.f },
        { L"Eve_Attack_SL12 (Animation Clip)", L"Eve_Attack_SL12", 4, 20, 24, false, 0.f },
        { L"Eve_Attack_LS23 (Animation Clip)", L"Eve_Attack_LS23", 4, 18, 21, false, 0.f },
        { L"Eve_Attack_SL34 (Animation Clip)", L"Eve_Attack_SL34", 4, 20, 23, false, 0.f },
        { L"Eve_Attack_LS45 (Animation Clip)", L"Eve_Attack_LS45", 4, 32, 35, false, 0.f },
        { L"Eve_Attack_LLS23 (Animation Clip)", L"Eve_Attack_LLS23", 4, 21, 24, false, 0.f },
        { L"Eve_Attack_LLLS34 (Animation Clip)", L"Eve_Attack_LLLS34", 4, 21, 24, false, 0.f }
    };

    constexpr _uint kThrustEnableBoxFrame = 11;
    constexpr _float kThrustKnockbackAmount = 0.f;
    constexpr _float kComboInputBufferLife = 0.25f;
    constexpr _float kDashAttackHoldThreshold = 0.3f;

    _float GetAttackEnterKnockbackAmount(const _bool isStrong)
    {
        return isStrong ? kStrongAttackDescs[0].knockbackAmount : kLightAttackDescs[0].knockbackAmount;
    }
}

CPlayerState_Attack::CPlayerState_Attack()
    : m_bCanContinue(false)
    , m_bPressedContinue(false)
    , m_bComboTransitionQueued(false)
    , m_bBufferedComboInput(false)
    , m_bDashAttackHoldTracking(false)
    , m_bDashAttackRequested(false)
    , m_iCrtCombo(0)
    , m_bUnderTerm(false)
    , m_bUnderLimit(false)
    , m_iComboTerm()
    , m_iComboLimit()
    , m_iComboTerm_S()
    , m_iComboLimit_S()
    , m_fBufferedComboInputTime(0.f)
    , m_fDashAttackHoldTime(0.f)
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

    const _float endRate = 0.78f;

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

    const auto beginCombo = [this](const _bool canTurnOnStart, const _float knockbackAmount, const function<void()>& onStart)
    {
        m_pCtx->Get_Player()->OffSwordAttackHandler();

        ++m_iCrtCombo;

        const _uint maxCombo = m_bStrong ? 3u : 4u;
        if (m_iCrtCombo > maxCombo)
            m_iCrtCombo = 1;

        if (onStart)
            onStart();

        m_pCtx->Get_Player()->SetWeaponKnockbackAmount(knockbackAmount);

        m_pCtx->Animator()->SetInt(L"attackCombo", m_iCrtCombo);
        m_pCtx->Animator()->SetBool(L"comboContinue", false);
        m_bCanContinue = true;
        m_bPressedContinue = false;
        m_bUnderTerm = true;
        m_bUnderLimit = true;
        m_bComboTransitionQueued = false;

        if (m_bBufferedComboInput)
        {
            m_bPressedContinue = true;
            m_bBufferedComboInput = false;
            m_fBufferedComboInputTime = 0.f;
        }

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
                m_pCtx->Get_Player()->OffSwordAttackHandler();
                m_bCanContinue = false;
                m_bUnderLimit = false;
            });
    };

    const auto registerComboClip = [this, endRate, &registerActionTrigger, &beginCombo, &registerComboWindow](
        const wstring& clipName,
        const wstring& startTrigger,
        const wstring& enableBoxTrigger,
        const _uint enableBoxFrame,
        const wstring& termTrigger,
        const wstring& limitTrigger,
        const wstring& endTrigger,
        const _uint termFrame,
        const _uint limitFrame,
        const _bool canTurnOnStart,
        const _float knockbackAmount,
        const function<void()>& onStart) -> CAnimationClip*
    {
        CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(clipName);

        if (!clip)
            return (CAnimationClip*)nullptr;

        const _uint endFrame = clip->Get_NormalizedFrameIndex(endRate);

        registerActionTrigger(clip, 1, startTrigger, [beginCombo, canTurnOnStart, knockbackAmount, onStart]()
            {
                beginCombo(canTurnOnStart, knockbackAmount, onStart);
            });
        registerActionTrigger(clip, enableBoxFrame, enableBoxTrigger, [this]()
            {
                m_pCtx->Get_Player()->OnSwordAttackHandler();
            });
        registerComboWindow(clip, termFrame, termTrigger, limitFrame, limitTrigger);
        registerActionTrigger(clip, endFrame, endTrigger, [this]()
            {
                Exit();
            });

        return clip;
    };

    for (const AttackClipDesc& desc : kLightAttackDescs)
    {
        const wstring prefix = desc.triggerPrefix;

        registerComboClip
        (
            desc.clipName,
            prefix + L"_Enter",
            prefix + L"_EnableBox",
            desc.enableBoxFrame,
            prefix + L"_Term",
            prefix + L"_Limit",
            prefix + L"_Exit",
            desc.termFrame,
            desc.limitFrame,
            desc.canTurnOnStart,
            desc.knockbackAmount,
            nullptr
        );
    }

    for (const AttackClipDesc& desc : kStrongAttackDescs)
    {
        const wstring prefix = desc.triggerPrefix;

        registerComboClip
        (
            desc.clipName,
            prefix + L"_Enter",
            prefix + L"_EnableBox",
            desc.enableBoxFrame,
            prefix + L"_Term",
            prefix + L"_Limit",
            prefix + L"_Exit",
            desc.termFrame,
            desc.limitFrame,
            desc.canTurnOnStart,
            desc.knockbackAmount,
            nullptr
        );
    }

    for (const AttackSequenceDesc& desc : kAttackSequenceDescs)
    {
        const wstring prefix = desc.triggerPrefix;

        registerComboClip
        (
            desc.clipName,
            prefix + L"_Start",
            prefix + L"_EnableBox",
            desc.enableBoxFrame,
            prefix + L"_Term",
            prefix + L"_Limit",
            prefix + L"_End",
            desc.termFrame,
            desc.limitFrame,
            desc.canTurnOnStart,
            desc.knockbackAmount,
            nullptr
        );
    }

    CAnimationClip* thrustClip = registerComboClip
    (
        L"Eve_Attack_LLSS34 (Animation Clip)",
        L"Eve_Attack_Thrust_Start",
        L"Eve_Attack_Thrust_EnableBox",
        kThrustEnableBoxFrame,
        L"Eve_Attack_Thrust_Term",
        L"Eve_Attack_Thrust_Limit",
        L"Eve_Attack_Thrust_End",
        21,
        24,
        false,
        kThrustKnockbackAmount,
        [this]()
        {
            m_bThrust = true;
        }
    );

    if (thrustClip)
    {
        registerActionTrigger(thrustClip, 11, L"Eve_Attack_Thrust_Stop", [this]()
            {
                m_bThrust = false;
                m_pCtx->StopMoveImmediate();
            });
    }
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
    m_bComboTransitionQueued = false;
    m_bBufferedComboInput = false;
    m_fBufferedComboInputTime = 0.f;
    m_bThrust = false;
    m_bDashAttackHoldTracking = false;
    m_bDashAttackRequested = false;
    m_fDashAttackHoldTime = 0.f;

    m_pCtx->Get_Player()->OffSwordAttackHandler();
    m_pCtx->Get_Player()->SetWeaponKnockbackAmount(GetAttackEnterKnockbackAmount(m_bStrong));

    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(true);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(true);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(true);

    m_pCtx->Get_Player()->GetRigidBody()->ResetVelocity();
}

void CPlayerState_Attack::Update()
{
    __super::Update();

    if (m_bBufferedComboInput)
    {
        m_fBufferedComboInputTime += DELTA_TIME;

        if (m_fBufferedComboInputTime >= kComboInputBufferLife)
        {
            m_bBufferedComboInput = false;
            m_fBufferedComboInputTime = 0.f;
        }
    }

    const _bool isLightAttackDown = m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack);
    const _bool isStrongAttackDown = m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack_S);
    const _bool isStrongAttackHold = m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Attack_S);
    const _bool isStrongAttackUp = m_pCtx->IsKeyPressed_UP(CPlayerController::PlayerState::Attack_S);

    if (isStrongAttackDown && m_pCtx->Get_Player()->IsDashAttackReady())
    {
        m_bDashAttackHoldTracking = true;
        m_bDashAttackRequested = false;
        m_fDashAttackHoldTime = 0.f;
    }

    if (m_bDashAttackHoldTracking)
    {
        if (isStrongAttackHold)
        {
            m_fDashAttackHoldTime += DELTA_TIME;

            if (m_fDashAttackHoldTime >= kDashAttackHoldThreshold)
            {
                m_bDashAttackHoldTracking = false;
                m_bDashAttackRequested = true;
            }
        }
        else if (isStrongAttackUp)
        {
            m_bDashAttackHoldTracking = false;
            m_fDashAttackHoldTime = 0.f;
        }
    }

    if (isLightAttackDown)
        m_bStrong = false;
    if (isStrongAttackDown)
        m_bStrong = true;

    m_pCtx->Animator()->SetBool(L"isAttack", !m_bStrong);
    m_pCtx->Animator()->SetBool(L"isStrongAttack", m_bStrong);

    if (isLightAttackDown || isStrongAttackDown)
    {
        HandleComboInput(isStrongAttackDown);

        if (!m_bUnderLimit)
        {
            Enter();
            return;
        }
    }

    if (m_bThrust)
    {
        m_pCtx->AddPosition(m_pCtx->Get_Player()->GetTransform()->Get_Directions().forward * 10.f * DELTA_TIME);
    }

    if (m_bDashAttackRequested && !m_bUnderLimit)
    {
        Exit();
        m_pCtx->BufferAction(CPlayerController::PlayerState::DashAttack);
        return;
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

    m_pCtx->Get_Player()->OffSwordAttackHandler();
    m_bComboTransitionQueued = false;
    m_bBufferedComboInput = false;
    m_fBufferedComboInputTime = 0.f;
    m_bDashAttackHoldTracking = false;
    m_bDashAttackRequested = false;
    m_fDashAttackHoldTime = 0.f;

    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(false);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(false);
    m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(false);

    m_pCtx->Get_Player()->SetAbleNavAgent(false);
}

void CPlayerState_Attack::HandleComboInput(const _bool strongInput)
{
    m_bStrong = strongInput;

    if (m_bComboTransitionQueued)
    {
        m_bBufferedComboInput = true;
        m_fBufferedComboInputTime = 0.f;
        return;
    }

    if (m_bCanContinue && m_bUnderTerm)
    {
        m_bPressedContinue = true;
        return;
    }

    ContinueCombo();
}

void CPlayerState_Attack::ContinueCombo()
{
    if (m_bCanContinue)
    {
        m_pCtx->Animator()->SetBool(L"comboContinue", true);
        m_bComboTransitionQueued = true;
        m_bPressedContinue = false;
    }
}



