#include "epch.h"
#include "BTAction.h"

CBTAction::CBTAction()
	: m_fnAction(nullptr)
{
	m_strName = L"BTAction";
}

CBTAction::CBTAction(const ActionCallback& _callback)
	: m_fnAction(_callback)
{
	m_strName = L"BTAction";
}

CBTAction::~CBTAction()
{
}

void CBTAction::SetAction(const ActionCallback& _callback)
{
	m_fnAction = _callback;
}

BTState CBTAction::Update(AIContext& _ctx)
{
	if (!m_fnAction)
		return BTState::Failure;

	return m_fnAction(_ctx);
}
