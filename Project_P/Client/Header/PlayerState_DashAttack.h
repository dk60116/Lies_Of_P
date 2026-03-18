#pragma once
#include "PlayerState.h"

class CPlayerState_DashAttack final : public CPlayerState
{
public:
    CPlayerState_DashAttack();
    ~CPlayerState_DashAttack();

public:
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    vector2 m_vStartDirection, m_vDuringDirection;
    _bool m_bDash;
    _bool m_bAttackBuffer, m_bGuardBffer;
};

