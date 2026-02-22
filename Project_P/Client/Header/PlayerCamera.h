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
        _float trackingSpeed = 10.f;
        _float firstZoomSensor = 4.f;
        _float pitchMin = -35.f;
        _float pitchMax = 70.f;
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
    void LateUpdate() override;
    void Update() override;
    void OnDestroy() override;

public:
    const vector3 Get_ForwardVector();
    const _float Get_ForwardAngle();

private:
    _float LerpAngle(_float current, _float target, _float t);

private:
    PlayerCameraOptions m_sOptions;

    _float m_fBackOffset;
    _float m_fZoomSensor;

    _float m_fTargetYaw;
    _float m_fTargetPitch;

    _float m_fCurYaw;
    _float m_fCurPitch;

    _bool m_bMouseLocked;
    _bool m_bIgnoreNextDelta;
};