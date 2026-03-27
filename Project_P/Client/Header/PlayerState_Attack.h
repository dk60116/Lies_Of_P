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
    void Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type) override;
    void Enter() override;
    void Update() override;
    void Exit() override;

private:
    void HandleComboInput(_bool strongInput);
    void ContinueCombo();

private:
    _bool m_bCanContinue, m_bPressedContinue, m_bUnderTerm, m_bUnderLimit;
    _bool m_bComboTransitionQueued;
    _bool m_bBufferedComboInput;

    _uint m_iCrtCombo;
    _uint m_iComboTerm[4];
    _uint m_iComboLimit[4];
    _uint m_iComboTerm_S[3];
    _uint m_iComboLimit_S[3];
    _float m_fBufferedComboInputTime;

    _bool m_bStrong;
    _bool m_bThrust;
};

