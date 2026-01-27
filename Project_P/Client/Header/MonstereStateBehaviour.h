#pragma once

#include "epch.h"
#include "MonsterController.h"

class CMonstereStateBehaviour final : public UObject
{
public:
	CMonstereStateBehaviour();
	~CMonstereStateBehaviour();

public:
	void Init();
	void Enter();
	void Update();
	void Exit();

private:
	CMonsterController::MonsterState m_eState;
};

