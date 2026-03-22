#pragma once
#include "LayoutGroup.h"

NS_BEGIN(Engine)

class ENGINE_DLL CHorizontalLayoutGroup final : public CLayoutGroup
{
	friend class CGameObject;

private:
	CHorizontalLayoutGroup();
	~CHorizontalLayoutGroup();

private:
	static CHorizontalLayoutGroup* Create();
	CComponent* Clone() const override;

private:
	void ApplyLayout(const LayoutContext& context) override;
};

NS_END

