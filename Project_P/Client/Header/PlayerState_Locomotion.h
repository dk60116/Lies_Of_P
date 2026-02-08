#pragma once
#include "PlayerState.h"
#include "PlayerController.h"

class CPlayerState_Locomotion : public CPlayerState
{
public:
    CPlayerState_Locomotion();
    ~CPlayerState_Locomotion();

public:
    void SetChildren(const unordered_map<CPlayerController::PlayerState, CPlayerState*> _childList);

    void Initialize(CPlayerControllerContext* _context) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    void TransitionTo(CPlayerState* _next);

private:
    CPlayerState* m_pChild;
    unordered_map<CPlayerController::PlayerState, CPlayerState*> m_mChildList;
};

