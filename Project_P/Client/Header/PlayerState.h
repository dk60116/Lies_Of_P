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

    virtual void Initialize(CPlayerControllerContext* _ctx);
    virtual void Enter();
    virtual void Exit();
    virtual void Update() PURE;

protected:
    CPlayerControllerContext* m_pCtx;
    _float m_fPassedTime;
};

