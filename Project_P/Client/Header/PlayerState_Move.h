#pragma once
#include "PlayerState.h"

class CPlayerState_Move final : public CPlayerState
{
public:
    void Initialize(CPlayerControllerContext* _ctx) override;
    void Enter() override;
    void Update() override;
};

