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
    void Initialize(CPlayerControllerContext* _ctx) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    void TurnPlayer();

private:
    _int  m_iCombo, m_iPrevCombo;
    _bool m_bQueuedNext;
    _float m_fComboTerm[3];
    _float m_fComboLimit[3];
    _float m_fEndTime[4];
};

