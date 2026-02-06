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
};
