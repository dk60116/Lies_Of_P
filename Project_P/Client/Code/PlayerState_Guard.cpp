#include "cpch.h"
#include "PlayerState_Guard.h"

CPlayerState_Guard::CPlayerState_Guard()
	: m_bExitableTime(false)
	, m_bExit(false)
{
}

CPlayerState_Guard::~CPlayerState_Guard()
{
}

void CPlayerState_Guard::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Start (Animation Clip)");

		if (startClip)
		{
			const wstring clipName = startClip->Get_ResourceName();
			const _uint frameCount = startClip->Get_FrameCount();

			CAnimationClip::ActionTrigger at = { 10, L"Guard_ExitAbleTime" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Guard_ExitAbleTime", [this]()
				{
					m_bExitableTime = true;
					m_pCtx->SetCanTurn(false);
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Walk_Forward_Start (Animation Clip)");

		if (startClip)
		{
			const wstring clipName = startClip->Get_ResourceName();
			const _uint frameCount = startClip->Get_FrameCount();

			CAnimationClip::ActionTrigger at = { 5, L"Guard_Start" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Guard_Start", [this]()
				{
					m_pCtx-> SetCanMove(true);
					m_pCtx->SetCanTurn(true);
				});
		}
	}

	{
		CAnimationClip* idleClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Idle (Animation Clip)");

		if (idleClip)
		{
			{
				CAnimationClip::ActionTrigger atMove = { 5, L"Guard_CanMoveTime" };
				idleClip->Add_ActionTrigger(atMove);
				m_pCtx->Animator()->RegisterActionHandler(L"Guard_CanMoveTime", [this]()
					{
						m_pCtx->SetCanMove(true);
					});

				CAnimationClip::ActionTrigger atExit = { 2, L"Guard_Idle_ExitAbleTime" };
				idleClip->Add_ActionTrigger(atExit);
				m_pCtx->Animator()->RegisterActionHandler(L"Guard_Idle_ExitAbleTime", [this]()
					{
						m_bExitableTime = true;
						m_pCtx->SetCanTurn(false);
					});
			}
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Walk_Forward_During (Animation Clip)");

		if (startClip)
		{
			const wstring clipName = startClip->Get_ResourceName();
			const _uint frameCount = startClip->Get_FrameCount();

			CAnimationClip::ActionTrigger at = { 1, L"Guard_During" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Guard_During", [this]()
				{
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Walk_Forward_End (Animation Clip)");

		if (startClip)
		{
			const wstring clipName = startClip->Get_ResourceName();
			const _uint frameCount = startClip->Get_FrameCount();

			{
				CAnimationClip::ActionTrigger at = { 1, L"Guard_WalkEndStart" };
				startClip->Add_ActionTrigger(at);
				m_pCtx->Animator()->RegisterActionHandler(L"Guard_WalkEndStart", [this]()
					{
					});
			}
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_End (Animation Clip)");

		if (startClip)
		{
			const wstring clipName = startClip->Get_ResourceName();
			const _uint frameCount = startClip->Get_FrameCount();

			{
				CAnimationClip::ActionTrigger at = { 1, L"Guard_EndStart" };
				startClip->Add_ActionTrigger(at);
				m_pCtx->Animator()->RegisterActionHandler(L"Guard_EndStart", [this]()
					{
						m_pCtx->SetCanMove(false);
						m_pCtx->SetCanAttack(true);
					});
			}

			{
				CAnimationClip::ActionTrigger at = { 10, L"Guard_Exit" };
				startClip->Add_ActionTrigger(at);
				m_pCtx->Animator()->RegisterActionHandler(L"Guard_Exit", [this]()
					{
						if (m_bExit)
						{
							m_pCtx->StopMoveImmediate();
							Exit();
						}
					});
			}
		}
	}
}

void CPlayerState_Guard::Enter()
{
	__super::Enter();

	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanTurn(true);

	m_pCtx->SetBattle(true);
	m_pCtx->SetAnimMoveSpeed(0.f);
	m_pCtx->Animator()->SetTrigger(L"guard");
	m_pCtx->Animator()->SetBool(L"isGuard", true);
	m_pCtx->SetCanAttack(false);
	m_pCtx->StopMoveImmediate();

	m_bExitableTime = false;
	m_bExit = false;
}

void CPlayerState_Guard::Update()
{
	__super::Update();

	if (m_bExitableTime)
	{
		if (!m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Guard))
		{
			m_bExit = true;
			m_pCtx->Animator()->SetBool(L"isGuard", false);
			m_pCtx->StopMoveImmediate();
		}
		else
		{
			if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Guard))
				Enter();
		}
	}

	if (m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Guard))
	{
		if (m_pCtx->IsRunning())
			m_pCtx->SetAnimMoveSpeed(1.f);
		else
			m_pCtx->StopMoveImmediate();
	}
}

void CPlayerState_Guard::Exit()
{
	__super::Exit();

	m_pCtx->Animator()->SetBool(L"isGuard", false);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanMove(true);
	m_pCtx->SetAnimMoveSpeed(0.f);
	m_pCtx->SetCanAttack(true);
}
