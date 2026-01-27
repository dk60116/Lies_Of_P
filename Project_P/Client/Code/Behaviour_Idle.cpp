#include "cpch.h"
#include "Behaviour_Idle.h"

CBehaviour_Idle::CBehaviour_Idle()
	: m_fChangeRandomTime(0.f)
{
}

CBehaviour_Idle::~CBehaviour_Idle()
{
}

HRESULT CBehaviour_Idle::Initialize(CMonster* _monster)
{
	if (FAILED(__super::Initialize(_monster)))
		return E_FAIL;

	return S_OK;
}

void CBehaviour_Idle::Enter()
{
	__super::Enter();

	m_fChangeRandomTime = CRandom::GetInstance().Range(2.f, 3.f);
	m_pMonster->Get_Animator()->SetLoop(true);
	m_pMonster->Get_Animator()->Play(L"Idle", 0.1f);
}

void CBehaviour_Idle::During()
{
	__super::During();

	if (m_fPassedTime >= m_fChangeRandomTime)
		m_pMonster->Change_State(CMonsterController::Patrole);
}

void CBehaviour_Idle::Exit()
{
	__super::Exit();
}
