#include "cpch.h"
#include "Behaviour_Find.h"

CBehaviour_Find::CBehaviour_Find()
{
}

CBehaviour_Find::~CBehaviour_Find()
{
}

HRESULT CBehaviour_Find::Initialize(CMonster* _monster)
{
	if (FAILED(__super::Initialize(_monster)))
		return E_FAIL;

	return S_OK;
}

void CBehaviour_Find::Enter()
{
	__super::Enter();

	m_pMonster->Get_Animator()->SetLoop(false);
	m_pMonster->Get_Animator()->Play(L"Find", 0.1f);
}

void CBehaviour_Find::During()
{
	__super::During();

	if (m_pMonster->Get_Animator()->Get_StateInfo().normalizeTime >= 0.9f)
	{
		m_pMonster->Change_State(CMonsterController::Tracking);
	}
}

void CBehaviour_Find::Exit()
{
	__super::Exit();
}
