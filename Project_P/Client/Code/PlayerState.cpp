#include "cpch.h"
#include "PlayerState.h"

CPlayerState::CPlayerState()
	: m_pCtx(nullptr)
	, m_eStateType(CPlayerController::PlayerState::Count)
	, m_fPassedTime(0.f)
{
}

CPlayerState::~CPlayerState()
{
}

void CPlayerState::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
	m_pCtx = _ctx;
	m_eStateType = _type;
}

void CPlayerState::Enter()
{
	m_pCtx->SetActionActive(m_eStateType, true);
	m_pCtx->SetCurrentState(m_eStateType);

	m_fPassedTime = 0.f;
}

void CPlayerState::Exit()
{
	m_pCtx->SetActionActive(m_eStateType, false);

	m_fPassedTime = 0.f;
}

const CPlayerController::PlayerState CPlayerState::GetStateType() const
{
	return m_eStateType;
}

void CPlayerState::Update()
{
	m_fPassedTime += DELTA_TIME;
}
