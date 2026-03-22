#include "epch.h"
#include "Terrain.h"

CTerrain::CTerrain()
	: m_vRectSize(0, 0)
{
	m_strName = L"Terrain";
}

CTerrain::~CTerrain()
{
}

CTerrain* CTerrain::Create()
{
	return new CTerrain();
}

CComponent* CTerrain::Clone() const
{
	CTerrain* clone = new CTerrain();
	clone->m_vRectSize = m_vRectSize;
	return clone;
}

HRESULT CTerrain::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CTerrain::OnDestroy()
{
}
