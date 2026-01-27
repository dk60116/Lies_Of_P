#pragma once

#include "MonsterBehaviour.h"

class CBehaviour_Find final : public CMonsterBehaviour
{
	friend class CMonsterController;

private:
	CBehaviour_Find();
	~CBehaviour_Find();

public:
	HRESULT Initialize(class CMonster* _monster) override;

	void Enter() override;
	void During() override;
	void Exit() override;
};

