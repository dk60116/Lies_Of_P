#pragma once
#include "PlayerState.h"

class CPlayerState_Jump final : public CPlayerState
{
public:
    CPlayerState_Jump();
    ~CPlayerState_Jump();

public:
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    _bool m_bExitable;
    _bool m_bForward;
    vector3 m_vForwardDir;
};

