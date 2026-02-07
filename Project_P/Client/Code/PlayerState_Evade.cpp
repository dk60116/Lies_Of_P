#include "cpch.h"
#include "PlayerState_Evade.h"

CPlayerState_Evade::CPlayerState_Evade()
	: m_vStartDirection({})
	, m_bIsDash(false)
	, m_bForward(false)
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
			CAnimationClip::ActionTrigger at = { 14, L"Evade_Backward_Stop" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Evade_Backward_Stop", [this]()
				{
					m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
				});
		}

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

	_float x = 0, z = 0;

	if (m_pCtx->Animator()->GetFloat(L"dirX", x) && m_pCtx->Animator()->GetFloat(L"dirZ", z))
	{
		m_vStartDirection = vector2(x, z);
		m_bForward = m_vStartDirection.y != 0.f;
	}

	m_pCtx->Animator()->SetTrigger(L"evade");

	m_pCtx->SetCanAttack(false);
	m_pCtx->SetCanGuard(false);

	m_bIsDash = true;
}

void CPlayerState_Evade::Update()
{
	__super::Update();

	if (m_bIsDash)
		m_pCtx->AddPosition(m_pCtx->PlayerForward() * DELTA_TIME * 9.f * (m_bForward ? 1.f : -1.f));

	m_pCtx->Animator()->SetFloat(L"dirX", m_vStartDirection.x);
	m_pCtx->Animator()->SetFloat(L"dirZ", m_vStartDirection.y);
}

void CPlayerState_Evade::Exit()
{
	__super::Exit();
	m_pCtx->SetActionActive(PlayerState::Evade, false);

	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanGuard(true);
}
