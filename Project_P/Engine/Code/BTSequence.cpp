#include "epch.h"
#include "BTSequence.h"

CBTSequence::CBTSequence()
    : m_iCurrentIndex(0)
{
}

CBTSequence::~CBTSequence()
{
}

BTState CBTSequence::Update(AIContext& _ctx)
{
    while (m_iCurrentIndex < m_vChildren.size())
    {
        BTState result = m_vChildren[m_iCurrentIndex]->Update(_ctx);

        if (result == BTState::Running)
            return BTState::Running;

        if (result == BTState::Failure)
        {
            Reset();
            return BTState::Failure;
        }

        ++m_iCurrentIndex;
    }

    Reset();
    return BTState::Success;
}

void CBTSequence::Reset()
{
    for (auto& child : m_vChildren)
        child->Reset();

    m_iCurrentIndex = 0;
}
