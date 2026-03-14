#include "cpch.h"
#include "InterfaceManager.h"

CInterfaceManager::CInterfaceManager()
{
}

CInterfaceManager::~CInterfaceManager()
{
}

CInterfaceManager& CInterfaceManager::GetInstance()
{
	static CInterfaceManager instance;
	return instance;
}

void CInterfaceManager::Window_CursorOnOff()
{
}
