#include "cpch.h"
#include "PlayerState_Guard.h"

CPlayerState_Guard::CPlayerState_Guard()
{
}

CPlayerState_Guard::~CPlayerState_Guard()
{
}

void CPlayerState_Guard::Initialize(CPlayerControllerContext* _ctx)
{
	__super::Initialize(_ctx);
}

void CPlayerState_Guard::Enter()
{
	__super::Enter();
}

void CPlayerState_Guard::Update()
{
	__super::Update();
}

void CPlayerState_Guard::Exit()
{
	__super::Exit();
}
