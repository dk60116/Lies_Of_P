#include "cpch.h"
#include "PlayerState_Evade.h"

CPlayerState_Evade::CPlayerState_Evade()
	: m_vStartDirection({})
	, m_vDuringDirection({})
	, m_bIsDash(false)
	, m_bInputDir(false)
	, m_bAttackBuffer(false)
	, m_bGuardBffer(false)
{
}

CPlayerState_Evade::~CPlayerState_Evade()
{
}

void CPlayerState_Evade::Initialize(CPlayerControllerContext* _ctx)
{
	__super::Initialize(_ctx);

	const _uint stopTime = 12;
	const _uint endTime = 20;

	{
		CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Evade_Forward (Animation Clip)");

		const wstring clipName = startClip->Get_ResourceName();
		const _uint frameCount = startClip->Get_FrameCount();

		{
			CAnimationClip::ActionTrigger at = { stopTime, L"Evade_Forward_Stop" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Evade_Forward_Stop", [this]()
				{
					m_pCtx->SetCanTurn(false);

					m_bIsDash = false;

					if (m_bGuardBffer)
					{
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Guard, true);
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
						return;
					}
					if (m_bAttackBuffer)
					{
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Attack, true);
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
					}
				});
		}

		{
			CAnimationClip::ActionTrigger at = { endTime, L"Evade_Forward_End" };
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
			CAnimationClip::ActionTrigger at = { stopTime, L"Evade_Backward_Stop" };
			startClip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(L"Evade_Backward_Stop", [this]()
				{
					m_pCtx->SetCanTurn(false);

					m_bIsDash = false;

					if (m_bGuardBffer)
					{
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Guard, true);
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
						return;
					}
					if (m_bAttackBuffer)
					{
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Attack, true);
						m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
					}
				});
		}

		{
			CAnimationClip::ActionTrigger at = { endTime, L"Evade_Backward_End" };
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

	m_pCtx->SetActionActive(PlayerState::Evade, true);

	_float x = 0, z = 0;

	if (m_pCtx->Animator()->GetFloat(L"dirX", x) && m_pCtx->Animator()->GetFloat(L"dirZ", z))
	{
		m_vStartDirection = vector2(x, z);
		m_bInputDir = m_vStartDirection.y != 0.f;
	}

	m_pCtx->Animator()->SetTrigger(L"evade");

	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanAttack(false);
	m_pCtx->SetCanGuard(false);

	m_bIsDash = true;

	m_bAttackBuffer = false;
	m_bGuardBffer = false;

	m_pCtx->Animator()->GetBool(L"isInputDir", m_bInputDir);

	m_pCtx->Animator()->SetFloat(L"dirX", m_vStartDirection.x);
	m_pCtx->Animator()->SetFloat(L"dirZ", m_vStartDirection.y);
}

void CPlayerState_Evade::Update()
{
	__super::Update();

	if (m_bIsDash)
		m_pCtx->AddPosition(m_pCtx->PlayerForward() * DELTA_TIME * 9.f * (m_bInputDir ? 1.f : -1.f));

	if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Attack))
		m_bAttackBuffer = true;
	if (m_pCtx->IsKeyPressed_Down(CPlayerController::PlayerState::Guard))
		m_bGuardBffer = true;

	if (m_pCtx->IsBigTurn())
		m_pCtx->SetActionActive(CPlayerController::PlayerState::Evade, false);
}

void CPlayerState_Evade::Exit()
{
	__super::Exit();
	m_pCtx->SetActionActive(PlayerState::Evade, false);

	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanGuard(true);
}
