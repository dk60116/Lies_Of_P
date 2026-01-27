#pragma once

#include "epch.h"

class CMainScene final : public CScene
{
public:
	CMainScene();
	~CMainScene();

public:
	HRESULT Initialize() override;
	void Update() override;

private:
	CCamera* m_pMainCamera;
	CCanvas* m_pCanvas;
	CImage* m_pLogoImage;
};

