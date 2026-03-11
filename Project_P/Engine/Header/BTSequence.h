#pragma once
#include "BTComposite.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTSequence : public CBTComposite
{
public:
	CBTSequence();
	~CBTSequence();

public:
	BTState Update(AIContext& _ctx) override;
	void Reset() override;

private:
	size_t m_iCurrentIndex;
};

NS_END
