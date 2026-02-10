#include "cpch.h"
#include "PlayerState_Idle.h"

void CPlayerState_Idle::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Idle (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { frameCount - 1, L"Idle_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Idle_End", [this]()
				{
					m_pCtx->Animator()->Stop();
				});
		}
	}
}

void CPlayerState_Idle::Enter()
{
	__super::Enter();
	m_pCtx->SetAnimMoveSpeed(0.f);
}

void CPlayerState_Idle::Update()
{
	__super::Update();

	m_pCtx->TickMove();

	if (m_pCtx->IsBattle())
	{
		if (m_fPassedTime >= 4.f)
			m_pCtx->SetBattle(false);
	}

	if (!m_pCtx->Animator()->IsPlaying())
		m_pCtx->Animator()->Play();
}
