#include "cpch.h"
#include "PlayerState_Guard.h"

CPlayerState_Guard::CPlayerState_Guard()
	: m_bExitableTime(false)
{
}

CPlayerState_Guard::~CPlayerState_Guard()
{
}

void CPlayerState_Guard::Initialize(CPlayerControllerContext* _ctx)
{
	__super::Initialize(_ctx);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Start (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		CAnimationClip::ActionTrigger at = { 10, L"ExitAbleTime" };
		startClip->Add_ActionTrigger(at);
		m_pCtx->Animator()->RegisterActionHandler(L"ExitAbleTime", [this]()
			{
				m_bExitableTime = true;
				m_pCtx->SetCanTurn(false);
			});
	}

	{
		CAnimationClip* idleClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Guard_Idle (Animation Clip)");

		{
			CAnimationClip::ActionTrigger at = { 5, L"CanMoveTime" };
			idleClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"CanMoveTime", [this]()
				{
					m_pCtx->SetCanMove(true);
					m_pCtx->SetCanTurn(true);
				});
		}
	}
}

void CPlayerState_Guard::Enter()
{
	__super::Enter();

	m_pCtx->SetGuardActive(true);

	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanTurn(true);

	m_pCtx->SetAttackActive(false);
	m_pCtx->SetBattle(true);
	m_pCtx->SetAnimMoveSpeed(0.f);
	m_pCtx->Animator()->SetTrigger(L"guard");
	m_pCtx->Animator()->SetBool(L"isGuard", true);

	m_bExitableTime = false;
}

void CPlayerState_Guard::Update()
{
	__super::Update();

	if (!m_pCtx->IsGuardPressed() && m_bExitableTime)
		m_pCtx->SetGuardActive(false);
}

void CPlayerState_Guard::Exit()
{
	__super::Exit();

	m_pCtx->Animator()->SetBool(L"isGuard", false);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanMove(true);

	CDebug::LogError("Exit");
}
