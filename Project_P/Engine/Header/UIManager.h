#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CUIManager final
{
	SINGLETONCLASS(CUIManager);

public:
	HRESULT Initialize();
	void Release();

public:
	CMaterial* Get_UIGizmoMaterial() const;

private:
	CMaterial* m_pUIGizmoMaterial;
};

NS_END

