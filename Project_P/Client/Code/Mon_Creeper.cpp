#include "cpch.h"
#include "Mon_Creeper.h"
#include "BT_Creeper.h"
#include "MonsterController.h"
#include "HurtBox.h"

CMon_Creeper::CMon_Creeper()
{
	m_strMonsterName = L"Creeper";
}

CMon_Creeper::~CMon_Creeper()
{
}

CMon_Creeper* CMon_Creeper::Create()
{
	return new CMon_Creeper();
}

CComponent* CMon_Creeper::Clone() const
{
	CMon_Creeper* clone = new CMon_Creeper();

	return clone;
}

HRESULT CMon_Creeper::Initialize()
{
	m_vMaterialTransparent = { false, true };

	m_vHurtBoxInfoList.push_back({ L"FX_Tail_04_end", vector3::one() * 0.1f });

	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CBehaviourTree* bt = m_pGameObject->AddComponent<CBT_Creeper>();
	m_pController->Bind(this, bt);
	m_pController->Set_DefaultState(CMonsterController::MonsterState::Hide);

	m_pBodyCollider->SetCenter(vector3(0.f, 0.9f, 0.f));
	m_pBodyCollider->SetHeight(0.8f);

	m_sStatus.moveSpeed = 4.f;

	return S_OK;
}

void CMon_Creeper::Awake()
{
	__super::Awake();
}

void CMon_Creeper::Start()
{
	__super::Start();
}

void CMon_Creeper::Update()
{
	__super::Update();
}

void CMon_Creeper::FixedUpdate()
{
	__super::FixedUpdate();
}

void CMon_Creeper::OnDestroy()
{
	__super::OnDestroy();
}

void CMon_Creeper::SetAnimationAction()
{
	{
		CAnimationClip* ac = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Anim_Mon_Creeper_BaseAttack (Animation Clip)");

		const _uint end = ac->Get_NormalizedFrameIndex(0.78f);

		{
			CAnimationClip::ActionTrigger at = { 19, L"AttackBase_Start" };

			ac->Add_ActionTrigger(at);
			m_pAnimator->RegisterActionHandler(L"AttackBase_Start", [this]()
				{
					CDebug::LogError("START");
					m_mHurtBoxList[L"FX_Tail_04_end"]->EnableBox();
				});
		}

		{
			CAnimationClip::ActionTrigger at = { end, L"AttackBase_End" };

			ac->Add_ActionTrigger(at);
			m_pAnimator->RegisterActionHandler(L"AttackBase_End", [this]()
				{
					CDebug::LogError("End");
					m_mHurtBoxList[L"FX_Tail_04_end"]->DisableBox();
				});
		}
	}
}
