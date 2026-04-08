#include "cpch.h"
#include "PlayerState_DashAttack.h"
#include "Player.h"

CPlayerState_DashAttack::CPlayerState_DashAttack()
	: m_vStartDirection({})
	, m_vDuringDirection({})
	, m_bDash(false)
	, m_bAttackBuffer(false)
	, m_bGuardBffer(false)
{
}

CPlayerState_DashAttack::~CPlayerState_DashAttack()
{
}

void CPlayerState_DashAttack::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Attack_Thrust (Animation Clip)");

	if (clip)
	{
		const _uint endFrame = clip->Get_NormalizedFrameIndex(0.38f);

		const auto registerActionTrigger = [this, clip](const _uint frame, const wstring& triggerName, const function<void()>& handler)
		{
			CAnimationClip::ActionTrigger at = { frame, triggerName };
			clip->Add_ActionTrigger(at);
			m_pCtx->Animator()->RegisterActionHandler(triggerName, [this, handler]()
				{
					if (!m_pCtx->IsActionActive(m_eStateType))
						return;

					handler();
				});
		};

		registerActionTrigger(1, L"DashAttack_Start", [this]()
			{
				m_bDash = true;
			});

		registerActionTrigger(4, L"DashAttack_EnableBox", [this]()
			{
				m_pCtx->Get_Player()->OnSwordAttackHandler();
			});

		registerActionTrigger(11, L"DashAttack_Stop", [this]()
			{
				m_bDash = false;
				m_pCtx->StopMoveImmediate();
			});

		registerActionTrigger(13, L"DashAttack_DisableBox", [this]()
			{

			});

		registerActionTrigger(18, L"DashAttack_StartRun", [this]()
			{
				if (m_pCtx->IsKeyPressed_Hold(PlayerState::Move))
					Exit();
			});

		registerActionTrigger(endFrame, L"DashAttack_End", [this]()
			{
				Exit();
			});
	}
}

void CPlayerState_DashAttack::Enter()
{
	__super::Enter();

	m_pCtx->Get_Player()->StartDashAttackCooldown();
	m_pCtx->Get_Player()->SetWeaponKnockbackAmount(1.f);
	m_pCtx->Get_Player()->OffSwordAttackHandler();

	m_pCtx->SetBattle(true);
	m_pCtx->StopMoveImmediate();
	m_pCtx->Animator()->SetTrigger(L"dashAttack");
	m_pCtx->Animator()->SetBool(L"isStrongAttack", true);

	m_pCtx->SetCanDashAttack(false);
	m_pCtx->SetCanMove(false);
	m_pCtx->SetCanTurn(false);
	m_pCtx->SetCanAttack(false);
	m_pCtx->SetCanGuard(false);
	m_pCtx->SetCanJump(false);
	m_pCtx->SetCanEvade(false);

	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(true);
	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(true);
	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(true);
	m_pCtx->Get_Player()->GetRigidBody()->ResetVelocity();

	m_bDash = false;
	m_bAttackBuffer = false;
	m_bGuardBffer = false;
}

void CPlayerState_DashAttack::Update()
{
	__super::Update();

	if (m_bDash)
		m_pCtx->AddPosition(m_pCtx->PlayerForward() * 10.f * DELTA_TIME);
}

void CPlayerState_DashAttack::Exit()
{
	__super::Exit();

	m_bDash = false;

	m_pCtx->Get_Player()->OffSwordAttackHandler();
	m_pCtx->SetCanDashAttack(true);
	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanGuard(true);
	m_pCtx->SetCanJump(true);
	m_pCtx->SetCanEvade(true);

	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionX(false);
	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionY(false);
	m_pCtx->Get_Player()->GetRigidBody()->SetConstPositionZ(false);
	m_pCtx->Get_Player()->SetAbleNavAgent(false);

	m_pCtx->Animator()->SetBool(L"isStrongAttack", false);
}
