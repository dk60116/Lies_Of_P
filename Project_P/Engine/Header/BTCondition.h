#pragma once
#include <functional>
#include "BTNode.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTCondition final : public CBTNode
{
public:
	using ConditionCallback = std::function<_bool(AIContext&)>;

public:
	CBTCondition()
		: m_fnCondition(nullptr)
	{
		m_strName = L"BTCondition";
	}

	explicit CBTCondition(const ConditionCallback& _callback)
		: m_fnCondition(_callback)
	{
		m_strName = L"BTCondition";
	}

	~CBTCondition()
	{
	}

public:
	void SetCondition(const ConditionCallback& _callback)
	{
		m_fnCondition = _callback;
	}

	BTState Update(AIContext& _ctx) override
	{
		if (!m_fnCondition)
			return BTState::Failure;

		return m_fnCondition(_ctx) ? BTState::Success : BTState::Failure;
	}

private:
	ConditionCallback m_fnCondition;
};

NS_END
