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

	for (_int i = 0; i < 2; ++i)
	{
		CreateObject(MapObjectType::Building);
	}

	m_vObjectList[1].renderers[0]->CreateMeshInstancing(10);

	for (size_t i = 0; i < 10; ++i)
	{
		m_vObjectList[1].renderers[0]->SetInstancingPosition(i, vector3(50.f * i, 0.f, 0.f));
	}

	return S_OK;
}

void CMap_01_SilentStreet::Awake()
{
}
