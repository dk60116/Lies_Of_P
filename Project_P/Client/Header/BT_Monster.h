#pragma once
#include "BehaviourTree.h"
#include "BTNode.h"

class CMonster;
class CMonsterController;

class CBT_Monster abstract : public CBehaviourTree
{
protected:
	CBT_Monster();
	~CBT_Monster();

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void Update() override;
	void OnDestroy() override;

protected:
	CBTNode* CreateHideChaseRoot();
	CMonsterController* Get_Controller();
	CMonster* Get_Monster();

	BTState RunHide(AIContext& _ctx);
	BTState RunChase(AIContext& _ctx);
	BTState RunAttack(AIContext& _ctx);
	BTState RunHit(AIContext& _ctx);

private:
	void Resolve_References();

protected:
	CMonsterController* m_pController;
	_bool m_bChaseMoveActive;
	_float m_fAttackCooldown;
	_bool m_bHitAnimationObserved;
	_float m_fHitReactionTimeout;
	vector3 m_vPreviousMonsterPosition;
	_bool m_bHasPreviousMonsterPosition;
};
