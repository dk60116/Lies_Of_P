#pragma once
#include "BT_Monster.h"

class CBT_Creeper final : public CBT_Monster
{
	friend class CGameObject;

protected:
	CBT_Creeper();
	~CBT_Creeper();

private:
	static CBT_Creeper* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void Update() override;
	void OnDestroy() override;
};
