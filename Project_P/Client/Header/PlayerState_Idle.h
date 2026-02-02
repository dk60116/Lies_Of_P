#pragma once
#include "PlayerState.h"

class CPlayerState_Idle final : public CPlayerState
{
public:
    void Enter(CPlayerControllerContext& _ctx) override;
    void Update(CPlayerControllerContext& _ctx) override;
};

