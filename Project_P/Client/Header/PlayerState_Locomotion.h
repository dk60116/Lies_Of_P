#pragma once
#include "PlayerState.h"

class CPlayerState_Locomotion : public CPlayerState
{
public:
    CPlayerState_Locomotion();
    ~CPlayerState_Locomotion();

public:
    void SetChildren(CPlayerState* _idle, CPlayerState* _move, CPlayerState* _attack);

    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    void TransitionTo(CPlayerState* _next);

private:
    CPlayerState* m_pChild;
    CPlayerState* m_pIdle;
    CPlayerState* m_pMove;
    CPlayerState* m_pAttack;
};

