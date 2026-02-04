#pragma once
#include "PlayerState.h"

class CPlayerState_Attack final : public CPlayerState
{
public:
    CPlayerState_Attack();
    ~CPlayerState_Attack();

private:
    struct Step
    {
        _float duration;
        _float chainOpen;
        _float chainClose;
        const wchar_t* trigger;
    };

public:
    void Enter(CPlayerControllerContext& _ctx) override;
    void Update(CPlayerControllerContext& _ctx) override;
    void Exit(CPlayerControllerContext& _ctx) override;

private:
    void PlayStep(CPlayerControllerContext& _ctx, _int _idx);
    void TurnPlayer(CPlayerControllerContext& _ctx);

private:
    _int  m_iCombo;
    _bool m_bQueuedNext;
    _float m_fComboTerm[3];
    _float m_fEndTime[4];

    _bool m_bDash;
};

