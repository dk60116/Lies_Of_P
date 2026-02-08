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
    void ContinueCombo();

private:
    _bool m_bCanContinue, m_bPressedContinue, m_bUnderTerm, m_bUnderLimit;

    _uint m_iCrtCombo;
    _uint m_iComboTerm[4];
    _uint m_iComboLimit[4];
    _uint m_iTurnLock[4];

    _bool m_bLastContinue;
};

