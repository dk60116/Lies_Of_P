#pragma once
#include "PlayerState.h"

class CPlayerState_Locomotion : public CPlayerState
{
public:
    CPlayerState_Locomotion();
    ~CPlayerState_Locomotion();

public:
    void SetChildren(CPlayerState* _idle, CPlayerState* _move);

    void Enter(CPlayerControllerContext& _ctx) override;
    void Update(CPlayerControllerContext& _ctx) override;
    void Exit(CPlayerControllerContext& _ctx) override;

private:
    void TransitionTo(CPlayerControllerContext& _ctx, CPlayerState* _next);

private:
    CPlayerState* m_pIdle;
    CPlayerState* m_pMove;
    CPlayerState* m_pChild;
};

