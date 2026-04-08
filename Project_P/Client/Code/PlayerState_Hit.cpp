#include "cpch.h"
#include "PlayerState_Hit.h"

PlayerState_Hit::PlayerState_Hit()
	: m_bCanMove(false)
	, m_vKnockbackDir(vector3::zero())
	, m_vAnimationNames({})
{
}

PlayerState_Hit::~PlayerState_Hit()
{
}

void PlayerState_Hit::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	__super::Initialize(_ctx, _type);

	m_vAnimationNames.push_back(L"Eve_Hit_Stand_Light_Fw (Animation Clip)");
	m_vAnimationNames.push_back(L"Eve_Hit_Stand_Light_Bw (Animation Clip)");
	m_vAnimationNames.push_back(L"Eve_Hit_Stand_Light_Lw (Animation Clip)");
	m_vAnimationNames.push_back(L"Eve_Hit_Stand_Light_Rw (Animation Clip)");

	for (size_t i = 0; i < m_vAnimationNames.size(); ++i)
	{
		{
			CAnimationClip* ac = CResources::GetInstance().LoadOnScene<CAnimationClip>(m_vAnimationNames[i]);

			if (ac)
			{
				const _uint endFrame = ac->Get_NormalizedFrameIndex(0.78f);

				{
					CAnimationClip::ActionTrigger at = { 14, L"Hit_"+ to_wstring(i) + L"_Stop" };
					ac->Add_ActionTrigger(at);
					m_pCtx->Animator()->RegisterActionHandler(L"Hit_" + to_wstring(i) + L"_Stop", [this]()
						{
							StopHandler();
						});
				}

				{
					CAnimationClip::ActionTrigger at = { endFrame, L"Hit_" + to_wstring(i) + L"_End" };
					ac->Add_ActionTrigger(at);
					m_pCtx->Animator()->RegisterActionHandler(L"Hit_" + to_wstring(i) + L"_End", [this]()
						{
							Exit();
						});
				}
			}
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

	m_bCanMove = false;
	m_vKnockbackDir = m_pCtx->ConsumeHitKnockback();
	m_pCtx->StopMoveImmediate();

	_int hitDirection = 0;
	vector3 playerForward = m_pCtx->PlayerForward();
	playerForward.y = 0.f;

	if (playerForward.lengthSq() > 0.0001f && m_vKnockbackDir.lengthSq() > 0.0001f)
	{
		const vector3 normalizedForward = playerForward.normalized();
		const vector3 normalizedRight = m_pCtx->Get_Controller()->Get_GameObject()->GetTransform()->Get_Directions().right.normalized();
		const vector3 normalizedKnockback = m_vKnockbackDir.normalized();
		const _float forwardDot = vector3::dot(normalizedForward, normalizedKnockback);
		const _float rightDot = vector3::dot(normalizedRight, normalizedKnockback);

		if (fabsf(rightDot) >= fabsf(forwardDot))
			hitDirection = rightDot >= 0.f ? 3 : 2;
		else
			hitDirection = forwardDot >= 0.f ? 1 : 0;
	}

	m_pCtx->Animator()->SetInt(L"HitDirection", hitDirection);
	m_pCtx->Animator()->SetTrigger(L"Hit");
}

void PlayerState_Hit::Update()
{
	__super::Update();

	if (!m_bCanMove && m_vKnockbackDir.lengthSq() > 0.0001f)
	{
		const vector3 knockbackDir = m_vKnockbackDir.normalized();
		const _float remainDistance = m_vKnockbackDir.length();
		const _float moveDistance = min(remainDistance, m_pCtx->PlayerStatus().hitKnockbackSpeed * DELTA_TIME);
		m_pCtx->AddPosition(knockbackDir * moveDistance);
		m_vKnockbackDir = knockbackDir * max(0.f, remainDistance - moveDistance);
	}

	if (m_bCanMove)
	{
		if (m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move))
		{
			Exit();
		}
	}
}

void PlayerState_Hit::Exit()
{
	__super::Exit();

	m_vKnockbackDir = vector3::zero();
	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanEvade(true);
	m_pCtx->SetCanGuard(true);
	m_pCtx->SetCanJump(true);
}

void PlayerState_Hit::StopHandler()
{
	m_pCtx->SetCanMove(true);
	m_pCtx->SetCanTurn(true);
	m_pCtx->SetCanAttack(true);
	m_pCtx->SetCanEvade(true);
	m_pCtx->SetCanGuard(true);
	m_pCtx->SetCanJump(true);

	m_bCanMove = true;
	m_vKnockbackDir = vector3::zero();

	m_pCtx->StopMoveImmediate();
}










