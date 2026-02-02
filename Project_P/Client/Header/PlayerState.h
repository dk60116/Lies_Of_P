#pragma once

#include "cpch.h"
#include "PlayerControllerContext.h"

class CPlayerControllerContext;

class CPlayerState : public UObject
{
public:
    virtual ~CPlayerState() = default;

    virtual void Enter(CPlayerControllerContext&);
    virtual void Exit(CPlayerControllerContext&);
    virtual void Update(CPlayerControllerContext&) PURE;
};

