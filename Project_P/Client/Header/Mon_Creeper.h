#pragma once
#include "Monster.h"

class CMon_Creeper : public CMonster
{
	friend class CGameObject;

protected:
	CMon_Creeper();
	~CMon_Creeper();

protected:
	static CMon_Creeper* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void FixedUpdate() override;
	void OnDestroy() override;
};

