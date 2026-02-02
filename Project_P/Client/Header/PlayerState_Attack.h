#pragma once
#include "PlayerState.h"

class CPlayerState_Attack final : public CPlayerState
{
private:
    struct Step
    {
        _float duration;
        _float chainOpen;
        _float chainClose;
        const wchar_t* trigger;
    };

    Step  m_steps[3] =
    {
        { 0.55f, 0.20f, 0.42f, L"atk1" },
        { 0.60f, 0.22f, 0.45f, L"atk2" },
        { 0.70f, 0.00f, 0.00f, L"atk3" },
    };

public:
    void Enter(CPlayerControllerContext& _ctx) override;
    void Update(CPlayerControllerContext& _ctx) override;
    void Exit(CPlayerControllerContext& _ctx) override;

private:
    void PlayStep(CPlayerControllerContext& _ctx, _int _idx);

private:
    _int  m_iStep = 0;
    _float m_t = 0.f;
    _bool m_bQueuedNext = false;
};

