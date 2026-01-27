#pragma once

#include "MonsterBehaviour.h"

class CBehaviour_Combat final : public CMonsterBehaviour
{
	friend class CMonsterController;

private:
	CBehaviour_Combat();
	~CBehaviour_Combat();

public:
	HRESULT Initialize(class CMonster* _monster) override;

	void Enter() override;
	void During() override;
	void Exit() override;
};

