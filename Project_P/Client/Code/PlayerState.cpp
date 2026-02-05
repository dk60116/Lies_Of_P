#include "cpch.h"
#include "PlayerState.h"

CPlayerState::CPlayerState()
	: m_pCtx(nullptr)
	, m_fPassedTime(0.f)
{
}

CPlayerState::~CPlayerState()
{
}

void CPlayerState::Initialize(CPlayerControllerContext* _ctx)
{
	m_pCtx = _ctx;
}

void CPlayerState::Enter()
{
	m_fPassedTime = 0.f;
}

void CPlayerState::Exit()
{
	m_fPassedTime = 0.f;
}

void CPlayerState::Update()
{
	m_fPassedTime += DELTA_TIME;
}
