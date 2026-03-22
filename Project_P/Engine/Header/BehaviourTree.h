#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class CBTNode;

enum class BTState
{
	Success,
	Failure,
	Running
};

struct AIContext
{
	class CGameObject* owner = nullptr;
	class CGameObject* target = nullptr;

	_bool hasTarget = false;
	_bool canSeeTarget = false;
	_float distanceToTarget = 0.f;
};

class ENGINE_DLL CBehaviourTree : public CComponent
{
	friend class CGameObject;

public:
	CBehaviourTree();
	~CBehaviourTree();

private:
	static CBehaviourTree* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void Update() override;
	void Render_Gizmo() override;
	void OnDestroy() override;

public:
	void SetRoot(CBTNode* _root);
	CBTNode* GetRoot();
	const CBTNode* GetRoot() const;

	AIContext& GetContext();
	const AIContext& GetContext() const;

	void SetTarget(class CGameObject* _target);
	void ClearTarget();
	BTState GetLastState() const;

private:
	void ClearTree();
	void RefreshContext();

private:
	CBTNode* m_pRoot;
	AIContext m_tContext;
	BTState m_eLastState;
};

NS_END
