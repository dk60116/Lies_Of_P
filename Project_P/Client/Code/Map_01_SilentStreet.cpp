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

	m_vFloorList[1].renderers[0]->CreateMeshInstancing(2);
	m_vFloorList[1].renderers[0]->SetInstancingPosition(1, vector3(-52.8f, 0.f, -4.f));
	m_vFloorList[1].renderers[0]->SetInstancingRotation(1, vector3(0.f, 25.f, 0.f));

	return S_OK;
}

void CMap_01_SilentStreet::Awake()
{
}
