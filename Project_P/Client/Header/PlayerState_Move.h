#pragma once
#include "PlayerState.h"

class CPlayerState_Move final : public CPlayerState
{
public:
    void Enter() override;
    void Update() override;
};

