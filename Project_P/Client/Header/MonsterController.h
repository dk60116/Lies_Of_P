#pragma once
#include "Component.h"
#include "BehaviourTree.h"

class CMonster;

class CMonsterController final : public CComponent
{
	friend class CGameObject;

public:
	enum class MonsterState
	{
		Patrol,
		Hide,
		Detect,
		Chase,
		Battle
	};

protected:
	CMonsterController();
	~CMonsterController();

protected:
	static CMonsterController* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void LateUpdate() override;
	void OnDestroy() override;

public:
	void Bind(CMonster* _monster, CBehaviourTree* _bt);
	void Set_State(const MonsterState _state);
	void Set_DefaultState(const MonsterState _state);

	CMonster* Get_Monster() const;
	CBehaviourTree* Get_BT() const;
	MonsterState Get_State() const;
	MonsterState Get_DefaultState() const;
	const _bool Has_Target() const;

private:
	void Refresh_Target();
	MonsterState Resolve_State() const;

private:
	CMonster* m_pMonster;
	CBehaviourTree* m_pBT;
	MonsterState m_eState;
	MonsterState m_eDefaultState;
	_bool m_bHasTarget;
};
