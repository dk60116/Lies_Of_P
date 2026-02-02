#pragma once

#include "cpch.h"
#include "PlayerControllerContext.h"

class CPlayerControllerContext;

class CPlayerState : public UObject
{
public:
    CPlayerState();
    ~CPlayerState();

    virtual void Enter(CPlayerControllerContext&);
    virtual void Exit(CPlayerControllerContext&);
    virtual void Update(CPlayerControllerContext&) PURE;

protected:
    _float m_fPassedTime;
};

