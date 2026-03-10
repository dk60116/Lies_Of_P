#include "BTNode.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBTComposite : public CBTNode
{
protected:
	CBTComposite();
	~CBTComposite();

public:
	void AddChild(CBTNode* _child);

protected:
	vector<CBTNode*> m_vChildren;
};

NS_END
