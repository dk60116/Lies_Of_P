#pragma once
#include "Component.h"

NS_BEGIN(Engine)

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

class ENGINE_DLL CBehaviourTree final : public CComponent
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
};

NS_END

