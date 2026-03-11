#include "cpch.h"
#include "BT_Monster.h"
#include "BTAction.h"
#include "BTSelector.h"
#include "GameObject.h"
#include "Monster.h"
#include "MonsterController.h"
#include "NaviMeshAgent.h"
#include "Transform.h"

namespace
{
	_bool IsAttackAnimationPlaying(CMonster* _monster)
	{
		if (!_monster)
			return false;

		CAnimator* animator = _monster->GetAnimator();
		if (!animator)
			return false;

		CAnimationClip* currentClip = animator->Get_CurrentAnimation();
		if (!currentClip)
			return false;

		const wstring& clipName = currentClip->Get_ResourceName();
		return clipName.find(L"Attack") != wstring::npos;
	}

	_bool ShouldPlayRunTurnAnimation(CMonster* _monster, const vector3& _targetPosition)
	{
		if (!_monster)
			return false;

		CTransform* transform = _monster->Get_Transform();
		if (!transform)
			return false;

		vector3 toTarget = _targetPosition - transform->Get_Position();
		toTarget.y = 0.f;
		if (toTarget.lengthSq() <= 0.0001f)
			return false;

		vector3 forward = transform->Get_Directions().forward;
		forward.y = 0.f;
		if (forward.lengthSq() <= 0.0001f)
			return false;

		toTarget = toTarget.normalized();
		forward = forward.normalized();
		const _float facingDot = forward.x * toTarget.x + forward.y * toTarget.y + forward.z * toTarget.z;
		return facingDot < 0.995f;
	}
}

CBT_Monster::CBT_Monster()
	: m_pController(nullptr)
	, m_bChaseMoveActive(false)
	, m_fAttackCooldown(0.f)
{
}

CBT_Monster::~CBT_Monster()
{
}

HRESULT CBT_Monster::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	Resolve_References();
	return S_OK;
}

void CBT_Monster::Awake()
{
	Resolve_References();
	__super::Awake();
}

void CBT_Monster::OnEnable()
{
	m_bChaseMoveActive = false;
	m_fAttackCooldown = 0.f;
	__super::OnEnable();
}

void CBT_Monster::OnDisable()
{
	m_bChaseMoveActive = false;
	m_fAttackCooldown = 0.f;
	__super::OnDisable();
}

void CBT_Monster::Update()
{
	Resolve_References();
	__super::Update();
}

void CBT_Monster::OnDestroy()
{
	m_bChaseMoveActive = false;
	m_fAttackCooldown = 0.f;
	m_pController = nullptr;
	__super::OnDestroy();
}

CBTNode* CBT_Monster::CreateHideChaseRoot()
{
	CBTSelector* root = new CBTSelector();

	CBTAction* attackAction = new CBTAction([this](AIContext& _ctx)
		{
			return RunAttack(_ctx);
		});

	CBTAction* chaseAction = new CBTAction([this](AIContext& _ctx)
		{
			return RunChase(_ctx);
		});

	CBTAction* hideAction = new CBTAction([this](AIContext& _ctx)
		{
			return RunHide(_ctx);
		});

	root->AddChild(attackAction);
	root->AddChild(chaseAction);
	root->AddChild(hideAction);

	return root;
}

CMonsterController* CBT_Monster::Get_Controller()
{
	Resolve_References();
	return m_pController;
}

CMonster* CBT_Monster::Get_Monster()
{
	CMonsterController* controller = Get_Controller();
	if (!controller)
		return nullptr;

	return controller->Get_Monster();
}

BTState CBT_Monster::RunHide(AIContext& _ctx)
{
	UNREFERENCED_PARAMETER(_ctx);

	CMonsterController* controller = Get_Controller();
	CMonster* monster = Get_Monster();
	if (!controller || !monster)
		return BTState::Failure;

	if (controller->Get_State() != CMonsterController::MonsterState::Hide &&
		controller->Get_State() != controller->Get_DefaultState())
		return BTState::Failure;

	m_bChaseMoveActive = false;
	m_fAttackCooldown = 0.f;

	if (CAnimator* animator = monster->GetAnimator())
	{
		animator->SetFloat(L"speed", 0.f);
		if (!animator->IsPlaying())
			animator->Play();
	}

	CNaviMeshAgent* navAgent = monster->GetNaviAgent();
	if (navAgent)
	{
		navAgent->SetAlwaysLookAt(false);
		navAgent->ResetPath();
	}

	return BTState::Success;
}

BTState CBT_Monster::RunChase(AIContext& _ctx)
{
	CMonsterController* controller = Get_Controller();
	CMonster* monster = Get_Monster();
	if (!controller || !monster)
		return BTState::Failure;

	if (controller->Get_State() != CMonsterController::MonsterState::Chase)
		return BTState::Failure;

	if (!_ctx.target || !_ctx.hasTarget)
		return BTState::Failure;

	CNaviMeshAgent* navAgent = monster->GetNaviAgent();
	if (!navAgent)
		return BTState::Failure;

	if (IsAttackAnimationPlaying(monster))
	{
		m_bChaseMoveActive = false;
		navAgent->SetAlwaysLookAt(false);
		navAgent->ResetPath();
		if (CAnimator* animator = monster->GetAnimator())
			animator->SetFloat(L"speed", 0.f);
		return BTState::Success;
	}

	CTransform* targetTransform = _ctx.target->Get_Transform();
	if (!targetTransform)
		return BTState::Failure;

	const CMonster::MonsterStatus& status = monster->GetStatus();
	const _float baseStopDistance = 1.f + monster->GetRadius() + status.attackRange;
	const _float stopBuffer = max(0.5f, monster->GetRadius());
	const _float animStopDistance = baseStopDistance + stopBuffer;
	const _float animResumeDistance = animStopDistance + stopBuffer * 2.f;

	if (m_bChaseMoveActive)
		m_bChaseMoveActive = (_ctx.distanceToTarget > animStopDistance);
	else
		m_bChaseMoveActive = (_ctx.distanceToTarget > animResumeDistance);

	if (!m_bChaseMoveActive)
		m_bChaseMoveActive = true;

	if (CAnimator* animator = monster->GetAnimator())
	{
		animator->SetFloat(L"speed", 1.f);
		if (!animator->IsPlaying())
			animator->Play();
	}

	navAgent->SetAlwaysLookAt(false);
	navAgent->SetMoveSpeed(status.moveSpeed);
	navAgent->SetStoppingDistance(animStopDistance);
	navAgent->SetDestination(targetTransform->Get_Position());

	return BTState::Success;
}

BTState CBT_Monster::RunAttack(AIContext& _ctx)
{
	CMonsterController* controller = Get_Controller();
	CMonster* monster = Get_Monster();
	if (!controller || !monster)
		return BTState::Failure;

	if (controller->Get_State() != CMonsterController::MonsterState::Battle)
		return BTState::Failure;

	if (!_ctx.target || !_ctx.hasTarget)
		return BTState::Failure;

	CTransform* targetTransform = _ctx.target->Get_Transform();
	if (!targetTransform)
		return BTState::Failure;

	m_bChaseMoveActive = false;

	const CMonster::MonsterStatus& status = monster->GetStatus();
	const _float stopBuffer = max(0.5f, monster->GetRadius());
	const _float battleStopDistance = 2.f + monster->GetRadius() + status.attackRange + stopBuffer * 3.f;
	const _bool isAttackPlaying = IsAttackAnimationPlaying(monster);

	CNaviMeshAgent* navAgent = monster->GetNaviAgent();
	if (navAgent)
	{
		if (isAttackPlaying)
		{
			navAgent->SetAlwaysLookAt(false);
			navAgent->ResetPath();
		}
		else
		{
			navAgent->SetAlwaysLookAt(true);
			navAgent->SetStoppingDistance(battleStopDistance);
			navAgent->SetDestination(targetTransform->Get_Position());
		}
	}

	if (status.attackSpeed > 0.f)
		m_fAttackCooldown = max(0.f, m_fAttackCooldown - DELTA_TIME);

	if (CAnimator* animator = monster->GetAnimator())
	{
		const _bool shouldPlayRunTurn = !isAttackPlaying && ShouldPlayRunTurnAnimation(monster, targetTransform->Get_Position());
		animator->SetFloat(L"speed", shouldPlayRunTurn ? 1.f : 0.f);

		if (status.attackSpeed > 0.f && m_fAttackCooldown <= 0.f)
		{
			animator->SetTrigger(L"attack");
			m_fAttackCooldown = 1.f / status.attackSpeed;
		}

		if (!animator->IsPlaying())
			animator->Play();
	}

	return BTState::Success;
}

void CBT_Monster::Resolve_References()
{
	if (!m_pController && m_pGameObject)
		m_pController = m_pGameObject->GetComponent<CMonsterController>();
}






