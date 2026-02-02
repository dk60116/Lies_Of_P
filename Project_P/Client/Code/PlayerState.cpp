#include "cpch.h"
#include "PlayerState.h"

CPlayerState::CPlayerState()
	: m_fPassedTime(0.f)
{
}

CPlayerState::~CPlayerState()
{
}

void CPlayerState::Enter(CPlayerControllerContext&)
{
}

void CPlayerState::Exit(CPlayerControllerContext&)
{
	m_fPassedTime = 0.f;
}

void CPlayerState::Update(CPlayerControllerContext&)
{
	m_fPassedTime += DELTA_TIME;
}
