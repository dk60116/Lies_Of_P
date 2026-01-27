#include "cpch.h"
#include "Behaviour_Combat.h"

CBehaviour_Combat::CBehaviour_Combat()
{
}

CBehaviour_Combat::~CBehaviour_Combat()
{
}

HRESULT CBehaviour_Combat::Initialize(CMonster* _monster)
{
	if (FAILED(__super::Initialize(_monster)))
		return E_FAIL;

	return S_OK;
}

void CBehaviour_Combat::Enter()
{
	__super::Enter();

	m_pMonster->Get_Animator()->SetLoop(false);
	m_pMonster->Get_Animator()->Play(L"Attack01", 0.1f);
}

void CBehaviour_Combat::During()
{
	__super::During();
}

void CBehaviour_Combat::Exit()
{
	__super::Exit();
}
