#pragma once

#include "epch.h"

class CMonster abstract : public CComponent
{
	friend class CMonsterController;

public:
	struct MonsterStatus
	{
		_int maxHp = 3;
		_int crtHp = 0;
		_float moveSpeed = 3.f;
		_float rotateSpeed = 5.f;
		_float detectionRange = 10.f;
		_float attackRange = 3.f;
	};

protected:
	CMonster();
	~CMonster();

public:
	static CMonster* Create();
	HRESULT Initialize() override;

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy() override;

public:
	const wstring& GetMonsterName() const;

	CAnimator* Get_Animator();
	void Change_State(const _uint _state);
	const MonsterStatus& Get_Status();

protected:
	void CreateBody();
	void PaintTexture();
	void CreateAnimator();
	void CreateAI();

protected:
	wstring m_strMonsterName;
	vector<_bool> m_vMaterialTransparent;
	vector<CSkinnedMeshRenderer*> m_vMeshRenderers;
	CCapsuleCollider* m_pBodyCollider;
	CRigidBody* m_pRigidBody;
	CAnimator* m_pAnimator;
	CMonsterController* m_pController;
	CNaviMeshAgent* m_pNavAgent;

	MonsterStatus m_sStatus;
};

