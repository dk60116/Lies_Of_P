#pragma once
#include "MonsterBehaviour.h"
class CBehaviour_Idle final : public CMonsterBehaviour
{
	friend class CMonsterController;

private:
	CBehaviour_Idle();
	~CBehaviour_Idle();

public:
	HRESULT Initialize(class CMonster* _monster) override;

	void Enter() override;
	void During() override;
	void Exit() override;

private:
	_float m_fChangeRandomTime;
};

