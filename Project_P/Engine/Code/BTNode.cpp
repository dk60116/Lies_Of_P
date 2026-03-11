#include "epch.h"
#include "BTNode.h"

CBTNode::CBTNode()
{
}

CBTNode::~CBTNode()
{
}

BTState CBTNode::Update(AIContext& _ctx)
{
    return BTState::Failure;
}

void CBTNode::Reset()
{
}
