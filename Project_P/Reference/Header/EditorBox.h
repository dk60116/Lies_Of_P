#pragma once

#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL CEditorBox abstract : public UObject
{
protected:
	explicit CEditorBox();
	virtual ~CEditorBox() = default;

public:
	virtual HRESULT Initialize();
	virtual void Render() PURE;
	virtual void OnDestroy() PURE;

protected:
	void Toggle_Begin();
	void Toggle_End();

protected:
	wstring m_strBoxName;
};

NS_END

