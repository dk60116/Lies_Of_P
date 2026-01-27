#include "cpch.h"
#include "Behaviour_Patrole.h"

CBehaviour_Patrole::CBehaviour_Patrole()
	: m_fChangeRandomTime(0.f)
	, m_bLookAround(false)
{
}

CBehaviour_Patrole::~CBehaviour_Patrole()
{
}

HRESULT CBehaviour_Patrole::Initialize(CMonster* _monster)
{
	if (FAILED(__super::Initialize(_monster)))
		E_FAIL;

	return S_OK;
}

void CBehaviour_Patrole::Enter()
{
	__super::Enter();

	m_fChangeRandomTime = CRandom::GetInstance().Range(5.f, 7.f);
	m_pMonster->Get_Animator()->SetLoop(true);
	m_pMonster->Get_Animator()->Play(L"Walk", 0.1f);
}

void CBehaviour_Patrole::During()
{
	__super::During();

	if (m_fPassedTime >= m_fChangeRandomTime && !m_bLookAround)
	{
		m_bLookAround = true;
		m_pMonster->Get_Animator()->SetLoop(false);
		m_pMonster->Get_Animator()->Play(L"LookAround", 0.25f);
		m_fPassedTime = 0.f;
		m_fChangeRandomTime = CRandom::GetInstance().Range(5.f, 7.f);
	}

	if (m_bLookAround)
	{
		if (m_pMonster->Get_Animator()->Get_StateInfo().normalizeTime >= 0.7f)
		{
			m_bLookAround = false;
			m_pMonster->Get_Animator()->SetLoop(true);
			m_pMonster->Get_Animator()->Play(L"Walk", 0.25f);
		}
	}

	CTransform* playerTf = CGameManager::GetInstance().Get_Player()->Get_Transform();
	const vector3 playerPos = playerTf->Get_Position();
	const vector3 playerAngle = playerTf->Get_EulerAngles();
	
	CTransform* myTf = m_pMonster->Get_Transform();
	const vector3 myPos = myTf->Get_Position();
	const vector3 myAngle = myTf->Get_EulerAngles();

	if (vector3::Distance(myPos, playerPos) < m_pMonster->Get_Status().detectionRange)
	{
		m_pMonster->Change_State(CMonsterController::Find);
	}
}

void CBehaviour_Patrole::Exit()
{
	__super::During();
}
