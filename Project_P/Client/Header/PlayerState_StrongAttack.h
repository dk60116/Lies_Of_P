#pragma once

#include "PlayerState.h"

class CPlayerState_StrongAttack : public CPlayerState
{
public:
    CPlayerState_StrongAttack();
    ~CPlayerState_StrongAttack();

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
    void ContinueCombo();

private:
    _bool m_bCanContinue, m_bPressedContinue, m_bUnderTerm, m_bUnderLimit;

    _uint m_iCrtCombo;
    _uint m_iComboTerm[3];
    _uint m_iComboLimit[3];
    _uint m_iTurnLock[3];

    _bool m_bLastContinue;
};

