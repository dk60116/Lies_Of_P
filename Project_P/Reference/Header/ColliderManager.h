#pragma once
#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CColliderManager final
{
	SINGLETONCLASS(CColliderManager);

public:
	HRESULT Initialize();
	void Release();
};

NS_END

