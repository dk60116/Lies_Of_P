#pragma once

#include "epch.h"

class CPlayerCamera final : public CComponent
{
public:
	struct PlayerCameraOptions
	{
		_float HeightOffset = 5.f;
		_float lookHeightOffset = 3.f;
		_float zoomMin = 2.f;
		_float zoomMax = 10.f;
		_float trackingSpeed = 2.f;
		_float firstZoomSensor = 4.f;
		_float rotateSpeed = 180.f;
		_float pitchMin = -35.f;      // deg
		_float pitchMax =  70.f;      // deg
		_float zoomSpeed = 20.f;      // 기존 firstZoomSensor 대신
		_float pivotHeight = 1.6f;
		_float lookSensitivity = 0.15f;
	};

protected:
	CPlayerCamera();
	~CPlayerCamera();

public:
	static CPlayerCamera* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void LateUpdate() override;
	void OnDestroy() override;

private:
	class CPlayer* m_pPlayer;
	PlayerCameraOptions m_sOptions;
	_float m_fBackOffset, m_fZoomSensor;

	_float m_fYawDeg = 0.f;
	_float m_fPitchDeg = 15.f;

	_bool m_bMouseLocked = true;
	_bool m_bIgnoreNextDelta = true;
};

