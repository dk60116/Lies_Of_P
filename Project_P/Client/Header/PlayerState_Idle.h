#pragma once
#include "PlayerState.h"

class CPlayerState_Idle final : public CPlayerState
{
public:
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type);
    void Enter() override;
    void Update() override;
};

