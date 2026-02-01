#pragma once

#include "epch.h"

class CPlayerController final : public CComponent
{
public:
	enum KeyMapping { Forward, Back, Left, Right };

protected:
	CPlayerController();
	~CPlayerController();

public:
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
	void Set_Player(CPlayer* _player);
	void Set_Camera(CPlayerCamera* _cam);

public:
	const _bool IsRunning() const;

private:
	void Update_Key();
	void Update_Move();

private:
	CPlayer* m_pPlayer;
	CPlayerCamera* m_pPlayerCam;

private:
	_bool m_bRunning;

private:
	unordered_map<KeyMapping, _bool> m_mKeyHold;

	vector3 m_vMoveDirection;

	_float m_fTargetYaw, m_fCrtYaw;
	_bool m_bRotate;
};

