#include "cpch.h"
#include "PlayerState_Jump.h"

void CPlayerState_Jump::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Jump_End (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { 13, L"Jump_Down" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Jump_Down", [this]()
				{
					m_pCtx->Animator()->SetBool(L"isJump", false);
					ExitState();
				});
		}
	}
}

void CPlayerState_Jump::Enter()
{
	__super::Enter();

	m_pCtx->Animator()->SetTrigger(L"jump");
	m_pCtx->Animator()->SetBool(L"isJump", true);

	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanTurn(false);
	m_pCtx->SetCanAttack(false);
	m_pCtx->SetCanGuard(false);
}

void CPlayerState_Jump::Update()
{
	__super::Update();
}

void CPlayerState_Jump::Exit()
{
	__super::Exit();

	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanGuard(true);
}
