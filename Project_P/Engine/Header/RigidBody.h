#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRigidBody : public CComponent
{
protected:
	explicit CRigidBody();
	~CRigidBody();

private:
	static CRigidBody* Create();
	CComponent* Clone() const override;

public:
	CCollider* m_pCollider;
	void OnDestroy() override;
};

NS_END

