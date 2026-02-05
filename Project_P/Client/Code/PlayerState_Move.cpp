#include "cpch.h"
#include "PlayerState_Move.h"

void CPlayerState_Move::Enter()
{
    m_pCtx->SetAnimMoveSpeed(0.2f);
}

void CPlayerState_Move::Update()
{
    __super::Update();

    const vector3& dir = m_pCtx->GetMoveWorldDir();
    const _float speed = m_pCtx->PlayerStatus().moveSpeed;
    m_pCtx->AddPosition(dir * speed * DELTA_TIME);

    m_pCtx->BeginTurnTo(m_pCtx->GetDesiredYawDeg());

    if (m_fPassedTime >= 0.5f)
        m_pCtx->SetAnimMoveSpeed(1.f);
}
