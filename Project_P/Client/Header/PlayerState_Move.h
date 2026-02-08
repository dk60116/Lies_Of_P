#pragma once
#include "PlayerState.h"

class CPlayerState_Move final : public CPlayerState
{
public:
    CPlayerState_Move();
    ~CPlayerState_Move();

public:
    void Initialize(CPlayerControllerContext* _ctx) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    _bool m_bPrevSprint;
};

