#pragma once
#include "ColliderBox.h"

class CHitBox final : public CColliderBox
{
	friend class CGameObject;

protected:
	CHitBox();
	~CHitBox();

private:
	static CHitBox* Create();
	CComponent* Clone() const override;
};

