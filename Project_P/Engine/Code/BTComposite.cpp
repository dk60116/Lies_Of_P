#include "epch.h"
#include "BTComposite.h"

CBTComposite::CBTComposite()
{
}

CBTComposite::~CBTComposite()
{
}

void CBTComposite::AddChild(CBTNode* _child)
{
    m_vChildren.push_back(move(_child));
}