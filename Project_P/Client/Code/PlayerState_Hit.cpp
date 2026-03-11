#include "cpch.h"
#include "PlayerState_Hit.h"

PlayerState_Hit::PlayerState_Hit()
{
}

PlayerState_Hit::~PlayerState_Hit()
{
}

void PlayerState_Hit::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Hit_Stand_Light_Fw (Animation Clip)");

		const _uint endFrame = startClip->Get_NormalizedFrameIndex(0.78f);

		{
			CAnimationClip::ActionTrigger at = { 14, L"Hit0_Exit" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Hit0_Exit", [this]()
				{
					m_pCtx->SetCanMove(true);
					m_pCtx->SetCanTurn(true);
					m_pCtx->SetCanAttack(true);
					m_pCtx->SetCanEvade(true);
					m_pCtx->SetCanGuard(true);
					m_pCtx->SetCanJump(true);
				});
		}

		{
			CAnimationClip::ActionTrigger at = { endFrame, L"Hit0_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Hit0_End", [this]()
				{
					Exit();
				});
		}
	}
}

void PlayerState_Hit::Enter()
{
	__super::Enter();

	m_pCtx->SetBattle(true);

	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanTurn(false);
	m_pCtx->SetCanAttack(false);
	m_pCtx->SetCanEvade(false);
	m_pCtx->SetCanGuard(false);
	m_pCtx->SetCanJump(false);

	m_pCtx->Animator()->SetTrigger(L"Hit");
}

void PlayerState_Hit::Update()
{
	__super::Update();
}

void PlayerState_Hit::Exit()
{
	__super::Exit();

	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanEvade(true);
	m_pCtx->SetCanGuard(true);
	m_pCtx->SetCanJump(true);
}
