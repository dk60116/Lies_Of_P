#pragma once
#include "epch.h"

#include "PlayerControllerTypes.h"

class CPlayerState;

class CPlayerController final : public CComponent
{
    friend class CGameObject;
    friend class CPlayerControllerContext;

public:
    enum KeyMapping { Forward, Back, Left, Right, Attack, Attack_S, Guard, Evade, Jump, Hit };

    using PlayerState = ::PlayerState;

protected:
    CPlayerController();
    ~CPlayerController();

protected:
    static CPlayerController* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;

    void Awake() override;
    void Start() override;
    void Update() override;
    void LateUpdate() override;
    void OnDestroy() override;

public:
    CPlayerState* Get_PlayerState(PlayerState _state);
    void RequestAction(PlayerState _state);

public:
    void Set_Player(CPlayer* _player);
    void Set_Camera(CPlayerCamera* _cam);

public:
    const _bool IsRunning() const;
    const _bool IsBattle() const;
    void SetBattle(const _bool _value);

private:
    void Update_Key();

private:
    CPlayer* m_pPlayer;
    CPlayerCamera* m_pPlayerCam;

private:
    _bool m_bFSMStarted;

    CPlayerControllerContext* m_pCtx;
    CPlayerState* m_pRoot;

    unordered_map<PlayerState, CPlayerState*> m_mStateList;
    unordered_map<KeyMapping, _bool> m_mKeyHold, m_mKeyDown, m_mKeyUp;

private:
    _bool m_bRunning;
    _bool m_bBattleMode;

private:
    _float m_fPrevSpeed;
};
