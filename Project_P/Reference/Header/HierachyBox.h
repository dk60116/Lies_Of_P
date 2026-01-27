#pragma once

#include "EditorBox.h"

NS_BEGIN(Engine)

class ENGINE_DLL CHierachyBox final : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CHierachyBox();
	~CHierachyBox();

public:
	void Render() override;
	void OnDestroy() override;

private:
	static CHierachyBox* Create();

private:
	void RenderObjectHierarchy(CGameObject* _obj);
};

NS_END

