#pragma once

#include "cpch.h"
#include "PlayerControllerContext.h"
#include "PlayerController.h"

class CPlayerControllerContext;

class CPlayerState : public UObject
{
public:
    CPlayerState();
    ~CPlayerState();

    virtual void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type);
    virtual void Enter();
    virtual void Exit();
    virtual void Update() PURE;

protected:
    void ExitState();

protected:
    CPlayerControllerContext* m_pCtx;
    CPlayerController::PlayerState m_eStateType;
    _float m_fPassedTime;
};

