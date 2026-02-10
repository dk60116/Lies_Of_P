#pragma once

#include "Component.h"

class CSceneMap abstract : public CComponent
{
public:
	enum class MapObjectType { Building, Floor, Prop };

	struct MapObject
	{
		MapObjectType type = MapObjectType::Building;
		vector<CMeshRenderer*> renderers = {};
	};

protected:
	CSceneMap();
	~CSceneMap();

public:
	HRESULT Initialize() override;

	void OnDestroy() override;

public:
	void SetMapName(const wstring& _name);
	const wstring& GetMapName();

protected:
	void CreateObject(const MapObjectType _type);

protected:
	_uint m_iMapIndex;
	wstring m_strMapName;
	CTransform* m_pParentTF;
	vector<MapObject> m_vBuildingList, m_vFloorList, m_vPropList;
};

