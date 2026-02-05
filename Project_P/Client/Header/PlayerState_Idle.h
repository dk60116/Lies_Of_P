#pragma once
#include "PlayerState.h"

class CPlayerState_Idle final : public CPlayerState
{
public:
    void Enter() override;
    void Update() override;
};

