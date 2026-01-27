#pragma once

#include "Monster.h"

class CWolf final : public CMonster
{
protected:
	CWolf();
	~CWolf();

public:
	static CWolf* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;
};

