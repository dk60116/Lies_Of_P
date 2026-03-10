#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTNode abstract : public UObject
{
protected:
	CBTNode();
	~CBTNode();

public:
	virtual BTState Update(AIContext& _ctx) PURE;
	virtual void Reset();
};

NS_END

