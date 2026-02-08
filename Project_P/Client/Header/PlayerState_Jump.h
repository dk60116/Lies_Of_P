#pragma once
#include "PlayerState.h"

class CPlayerState_Jump final : public CPlayerState
{
public:
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type) override;
    void Enter() override;
    void Update() override;
    void Exit() override;
};

