#include "cpch.h"
#include "PlayerState_Move.h"

void CPlayerState_Move::Initialize(CPlayerControllerContext* _ctx)
{
    __super::Initialize(_ctx);


    {
        CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_SprintForward_During (Animation Clip)");

        const wstring clipName = startClip->Get_ResourceName();
        const _uint frameCount = startClip->Get_FrameCount();

        CAnimationClip::ActionTrigger at = { 16, L"ExitAbleTime11" };
        startClip->Add_ActionTrigger(at);
        m_pCtx->Animator()->RegisterActionHandler(L"ExitAbleTime11", [this]()
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
    const _float speed = m_pCtx->PlayerStatus().moveSpeed;
    m_pCtx->AddPosition(dir * speed * DELTA_TIME);

    m_pCtx->BeginTurnTo(m_pCtx->GetDesiredYawDeg());

    if (!m_pCtx->IsSprint())
        m_pCtx->SetAnimMoveSpeed(1.f);
    else
        m_pCtx->SetAnimMoveSpeed(2.f);

    if (!m_pCtx->Animator()->IsPlaying())
        m_pCtx->Animator()->Play();

    _float s = 0;
    _bool b = 0;

    if (m_pCtx->Animator()->GetBool(L"isBattle", b) && m_pCtx->Animator()->GetFloat(L"speed", s))
        CDebug::LogError(to_string((_int)b) + ", " + to_string((_int)s));
}
