#pragma once

#include "epch.h"

class CLoadingScene final : public CScene
{
public:
	CLoadingScene();
	~CLoadingScene();

public:
	HRESULT Initialize() override;
	void Awake() override;
	void Update() override;

private:
	CCamera* m_pMainCamera;
	CCanvas* m_pCanvas;
	CImage* m_pLogoImage;

	_float m_fLoadGauge;
};

