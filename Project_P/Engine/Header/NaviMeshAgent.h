#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CNaviMeshAgent final : public CComponent
{
	friend class CGameObject;

protected:
	CNaviMeshAgent();
	~CNaviMeshAgent();

private:
	static CNaviMeshAgent* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void Update() override;
	void OnDestroy() override;
};

NS_END

