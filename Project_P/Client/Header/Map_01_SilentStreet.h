#pragma once
#include "SceneMap.h"

class CMap_01_SilentStreet final : public CSceneMap
{
protected:
	CMap_01_SilentStreet();
	~CMap_01_SilentStreet();

public:
	static CMap_01_SilentStreet* Create();
	CMap_01_SilentStreet* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
};

