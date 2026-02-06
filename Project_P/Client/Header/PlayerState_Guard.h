#pragma once
#include "PlayerState.h"

class CPlayerState_Guard final : public CPlayerState
{
public:
    CPlayerState_Guard();
    ~CPlayerState_Guard();

public:
    void Initialize(CPlayerControllerContext* _ctx) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    _bool m_bExitableTime, m_bExit;
};

