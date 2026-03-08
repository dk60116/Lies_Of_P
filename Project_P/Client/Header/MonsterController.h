#pragma once
#include "Component.h"

class CMonsterController final : public CComponent
{
	friend class CGameObject;

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
};

