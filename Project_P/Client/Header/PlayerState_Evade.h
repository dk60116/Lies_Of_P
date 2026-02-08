#pragma once
#include "PlayerState.h"

class CPlayerState_Evade final : public CPlayerState
{
public:
    CPlayerState_Evade();
    ~CPlayerState_Evade();

public:
    void Initialize(CPlayerControllerContext* _ctx) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    vector2 m_vStartDirection, m_vDuringDirection;
    _bool m_bIsDash;
    _bool m_bInputDir;

    _bool m_bAttackBuffer, m_bGuardBffer;
};
