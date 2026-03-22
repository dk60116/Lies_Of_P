#pragma once

#include "Camera.h"

NS_BEGIN(Engine)

class CEditorCamera final : public CCamera
{
    friend class CGameObject;

public:
    struct EditorCameraOption
    {
        _float moveSpeed = 6.f;
        _float crtMoveSpeed = 0.f;
        _float rotateSpeed = 25.f;
        _float dragSpeed = 2.f;
        _float crtDragSpeed = 0.f;
        _float zoomSpeed = 8.f;
    };

	CEditorCamera();
	~CEditorCamera();

public:
    HRESULT Initialize() override;
	void Update_Editor() override;
	void Update() override;
    void OnDestroy() override;

private:
	static CEditorCamera* Create();
    CComponent* Clone() const override;

    EditorCameraOption m_sOptions;
    _bool m_bRMouseDowned;
    vector2 m_v2MouseDragDelta;
    vector2Int m_v2PrevMosuePos;
    float m_fPitch, m_fYaw;
    _bool m_bMoving;
    float m_fMoveTimeTotal, m_fMoveTimeCur;
    vector3 m_v3MoveFrom, m_v3MoveTo;
    _bool m_bZoomDistToggle;
};

NS_END

