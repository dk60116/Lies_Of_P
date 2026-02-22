#include "cpch.h"
#include "PlayerState_Jump.h"

CPlayerState_Jump::CPlayerState_Jump()
	: m_bExitable(false)
{
}

CPlayerState_Jump::~CPlayerState_Jump()
{
}

void CPlayerState_Jump::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	const _uint downTime = 13;
	const _uint EndTime = 50;

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Jump_End (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { downTime, L"Jump_Down" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Jump_Down", [this]()
				{
					m_bExitable = true;
				});
		}

		{
			CAnimationClip::ActionTrigger at = { EndTime, L"Jump_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Jump_End", [this]()
				{
					Exit();
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_JumpForward_End (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { downTime, L"JumpForward_Down" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"JumpForward_Down", [this]()
				{
					m_bExitable = true;
				});
		}

		{
			CAnimationClip::ActionTrigger at = { EndTime, L"JumpForward_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"JumpForward_End", [this]()
				{
					Exit();
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Battle_Jump_End (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { downTime, L"Battle_Jump_Down" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Battle_Jump_Down", [this]()
				{
					m_bExitable = true;
				});
		}

		{
			CAnimationClip::ActionTrigger at = { EndTime, L"Battle_Jump_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Battle_Jump_End", [this]()
				{
					Exit();
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Battle_JumpForward_End (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { downTime, L"Battle_JumpForward_Down" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Battle_JumpForward_Down", [this]()
				{
					m_bExitable = true;
				});
		}

		{
			CAnimationClip::ActionTrigger at = { EndTime, L"Battle_JumpForward_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Battle_JumpForward_End", [this]()
				{
					Exit();
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

	m_bExitable = false;

	m_pCtx->RigidBody()->AddForceY(m_pCtx->PlayerStatus().jumpPower);
}

void CPlayerState_Jump::Update()
{
	__super::Update();

	if (m_bExitable)
	{
		if (m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move))
			Exit();
		if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Jump))
			Enter();
	}
}

void CPlayerState_Jump::Exit()
{
	__super::Exit();

	m_pCtx->StopMoveImmediate();
	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanGuard(true);

	m_pCtx->Animator()->SetBool(L"isJump", false);
}
