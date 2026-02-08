#include "cpch.h"
#include "PlayerState_Move.h"

CPlayerState_Move::CPlayerState_Move()
    : m_bPrevSprint(false)
{
}

CPlayerState_Move::~CPlayerState_Move()
{
}

void CPlayerState_Move::Initialize(CPlayerControllerContext* _ctx)
{
    __super::Initialize(_ctx);


    {
        CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_SprintForward_During (Animation Clip)");

        const wstring clipName = clip->Get_ResourceName();
        const _uint frameCount = clip->Get_FrameCount();

        CAnimationClip::ActionTrigger at = { 16, L"Sprint_During_End" };
        clip->Add_ActionTrigger(at);
        m_pCtx->Animator()->RegisterActionHandler(L"Sprint_During_End", [this]()
            {
                m_pCtx->Animator()->Stop();
            });
    }

    {
        CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_BattleSprintForward_During (Animation Clip)");

        const wstring clipName = clip->Get_ResourceName();
        const _uint frameCount = clip->Get_FrameCount();

        CAnimationClip::ActionTrigger at = { 16, L"BattleSprint_During_End" };
        clip->Add_ActionTrigger(at);
        m_pCtx->Animator()->RegisterActionHandler(L"BattleSprint_During_End", [this]()
            {
                m_pCtx->Animator()->Stop();
            });
    }
}

void CPlayerState_Move::Enter()
{
    __super::Enter();
}

void CPlayerState_Move::Update()
{
    __super::Update();

    const vector3& dir = m_pCtx->GetMoveWorldDir();
    const _float speed = m_pCtx->IsSprint() ? m_pCtx->PlayerStatus().sprintSpeed : m_pCtx->PlayerStatus().runSpeed;
    m_pCtx->AddPosition(dir * speed * DELTA_TIME);

    m_pCtx->BeginTurnTo(m_pCtx->GetDesiredYawDeg());

    if (!m_pCtx->IsSprint())
        m_pCtx->SetAnimMoveSpeed(1.f);
    else
        m_pCtx->SetAnimMoveSpeed(2.f);

    if (!m_pCtx->Animator()->IsPlaying())
        m_pCtx->Animator()->Play();

    if (m_bPrevSprint && !m_pCtx->IsSprint())
    {
        if (!m_pCtx->IsSprint())
            m_pCtx->Animator()->SetTrigger(L"sprintToRun");
    }

    m_bPrevSprint = m_pCtx->IsSprint();
}

void CPlayerState_Move::Exit()
{
    __super::Exit();
}
