#include "epch.h"
#include "BTComposite.h"

CBTComposite::CBTComposite()
{
}

CBTComposite::~CBTComposite()
{
    for (auto& child : m_vChildren)
        Safe_Release(child);

    m_vChildren.clear();
}

void CBTComposite::AddChild(CBTNode* _child)
{
    if (!_child)
        return;

    _child->AddRef();
    m_vChildren.push_back(_child);
}
