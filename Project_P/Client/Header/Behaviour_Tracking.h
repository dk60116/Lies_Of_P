#pragma once
#include "MonsterBehaviour.h"

class CBehaviour_Tracking final : public CMonsterBehaviour
{
	friend class CMonsterController;

private:
	CBehaviour_Tracking();
	~CBehaviour_Tracking();

public:
	HRESULT Initialize(class CMonster* _monster) override;

	void Enter() override;
	void During() override;
	void Exit() override;
};

