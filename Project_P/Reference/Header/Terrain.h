#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CTerrain final : public CComponent
{
	friend class CGameObject;

private:
	vector2Int m_vRectSize;
};

NS_END

