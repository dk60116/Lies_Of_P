#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMainProcess final
{
	SINGLETONCLASS(CMainProcess);

public:
	HRESULT Initialize();
	void Update_MainApp();
	void Release_MainApp();
};

NS_END

