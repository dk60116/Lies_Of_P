#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CTerrain final : public CComponent
{
	friend class CGameObject;

protected:
	explicit CTerrain();
	~CTerrain();

private:
	static CTerrain* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void OnDestroy() override;

private:
	vector2Int m_vRectSize;
};

NS_END
