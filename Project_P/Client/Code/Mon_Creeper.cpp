#include "cpch.h"
#include "Mon_Creeper.h"
#include "BT_Creeper.h"
#include "MonsterController.h"
#include "HurtBox.h"

CMon_Creeper::CMon_Creeper()
{
	m_strCharacterName = L"Creeper";
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

	m_vHurtBoxInfoList.push_back({ L"FX_Tail_04_end", vector3::one() * 30.f });

	m_vHitBoxInfoList.push_back({ L"Bip001-Pelvis", vector3::one() * 50.f, vector3::up() * 20.f });

	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CBehaviourTree* bt = m_pGameObject->AddComponent<CBT_Creeper>();
	m_pController->Bind(this, bt);
	m_pController->Set_DefaultState(CMonsterController::MonsterState::Hide);

	m_pBodyCollider->SetCenter(vector3(0.f, 2.5f, 0.f));
	m_pBodyCollider->SetHeight(2.5f);
	m_pBodyCollider->SetRadius(1.5f);

	m_sStatus.maxHp = 10000;
	m_sStatus.moveSpeed = 6.f;
	m_sStatus.attackRange = 0.5f;
	m_sStatus.attackPower = 80;

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
					m_mHurtBoxList[L"FX_Tail_04_end"]->EnableBox();
				});
		}

		{
			CAnimationClip::ActionTrigger at = { end, L"AttackBase_End" };

			ac->Add_ActionTrigger(at);
			m_pAnimator->RegisterActionHandler(L"AttackBase_End", [this]()
				{
					m_mHurtBoxList[L"FX_Tail_04_end"]->DisableBox();
				});
		}
	}
}
