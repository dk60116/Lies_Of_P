#pragma once
#include "PlayerState.h"

class PlayerState_Hit : public CPlayerState
{
public:
    PlayerState_Hit();
    ~PlayerState_Hit();

public:
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type) override;
    void Enter() override;
    void Update() override;
    void Exit() override;
};

