#pragma once
#include "LayoutGroup.h"

NS_BEGIN(Engine)

class ENGINE_DLL CVerticalLayoutGroup final : public CLayoutGroup
{
	friend class CGameObject;

private:
	CVerticalLayoutGroup();
	~CVerticalLayoutGroup();

private:
	static CVerticalLayoutGroup* Create();
	CComponent* Clone() const override;

private:
	void ApplyLayout(const LayoutContext& context) override;
};

NS_END

