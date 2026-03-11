#pragma once
#include <functional>
#include "BTNode.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTAction final : public CBTNode
{
public:
	using ActionCallback = std::function<BTState(AIContext&)>;

public:
	CBTAction();
	explicit CBTAction(const ActionCallback& _callback);
	~CBTAction();

public:
	void SetAction(const ActionCallback& _callback);
	BTState Update(AIContext& _ctx) override;

private:
	ActionCallback m_fnAction;
};

NS_END
