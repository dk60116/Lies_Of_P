#include "epch.h"
#include "BTSelector.h"

CBTSelector::CBTSelector()
    : m_iCurrentIndex(0)
{
}

CBTSelector::~CBTSelector()
{
}

BTState CBTSelector::Update(AIContext& _ctx)
{
    while (m_iCurrentIndex < m_vChildren.size())
    {
        BTState result = m_vChildren[m_iCurrentIndex]->Update(_ctx);

        if (result == BTState::Running)
            return BTState::Running;

        if (result == BTState::Success)
        {
            Reset();
            return BTState::Success;
        }

        ++m_iCurrentIndex;
    }

    Reset();
    return BTState::Failure;
}

void CBTSelector::Reset()
{
}
