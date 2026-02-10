#include "cpch.h"
#include "Map_01_SilentStreet.h"

CMap_01_SilentStreet::CMap_01_SilentStreet()
{
	m_iMapIndex = 1;
	m_strMapName = L"Silent Street";
}

CMap_01_SilentStreet::~CMap_01_SilentStreet()
{
}

CMap_01_SilentStreet* CMap_01_SilentStreet::Create()
{
	return new CMap_01_SilentStreet();
}

CMap_01_SilentStreet* CMap_01_SilentStreet::Clone() const
{
	CMap_01_SilentStreet* clone = new CMap_01_SilentStreet();

	return clone;
}

HRESULT CMap_01_SilentStreet::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	for (_int i = 0; i < 6; ++i)
		CreateObject(MapObjectType::Building);
	for (_int i = 0; i < 2; ++i)
		CreateObject(MapObjectType::Floor);

	for (auto* renderer : m_vFloorList[1].renderers)
	{
		if (!renderer)
			continue;

		renderer->CreateMeshInstancing(2);
		renderer->SetInstancingPosition(1, vector3(-34.8f, 3.315f, 10.72f));
		renderer->SetInstancingRotation(1, vector3(-19.8f, 66.82f, 1.48f));
	}

	return S_OK;
}

void CMap_01_SilentStreet::Awake()
{
}
