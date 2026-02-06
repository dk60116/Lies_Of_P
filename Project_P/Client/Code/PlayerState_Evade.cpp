#include "cpch.h"
#include "PlayerState_Evade.h"

CPlayerState_Evade::CPlayerState_Evade()
{
}

CPlayerState_Evade::~CPlayerState_Evade()
{
}

void CPlayerState_Evade::Initialize(CPlayerControllerContext* _ctx)
{
	__super::Initialize(_ctx);

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Evade_Forward (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { 28, L"Evade_Forward_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Evade_Forward_End", [this]()
				{
					m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
				});
		}
	}

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Evade_Backward (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { 28, L"Evade_Backward_End" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Evade_Backward_End", [this]()
				{
					m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
				});
		}
	}
}

void CPlayerState_Evade::Enter()
{
	__super::Enter();

	m_pCtx->SetBattle(true);
	m_pCtx->SetActionActive(PlayerState::Evade, true);

	_float x = 0, y = 0;

	if (m_pCtx->Animator()->GetFloat(L"dirX", x) && m_pCtx->Animator()->GetFloat(L"dirZ", y))
		CDebug::LogError(to_string((_int)x) + ", " + to_string((_int)y));

	m_pCtx->Animator()->SetTrigger(L"evade");
}

void CPlayerState_Evade::Update()
{
	__super::Update();
}

void CPlayerState_Evade::Exit()
{
	__super::Exit();
	m_pCtx->SetActionActive(PlayerState::Evade, false);
}
