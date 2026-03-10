#pragma once
#include "BTComposite.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTSelector : public CBTComposite
{
public:
	CBTSelector();
	~CBTSelector();

public:
	BTState Update(AIContext& _ctx) override;
	void Reset() override;

private:
	size_t m_iCurrentIndex;
};

NS_END

