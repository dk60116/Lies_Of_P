#include "cpch.h"
#include "SceneMap.h"

CSceneMap::CSceneMap()
	: m_iMapIndex(0)
	, m_strMapName({})
	, m_pParentTF(nullptr)
	, m_vBuildingList({})
	, m_vFloorList({})
	, m_vPropList({})
{
}

CSceneMap::~CSceneMap()
{
}

HRESULT CSceneMap::Initialize()
{
	CScene* scene = m_pGameObject->Get_Scene();

	CGameObject* parentObj = scene->Add_GameObject(m_strMapName);
	m_pParentTF = parentObj->GetTransform();

	m_pParentTF->SetParent(GetTransform());

	if (m_pParentTF)
		m_pParentTF->AddRef();

	return S_OK;
}

void CSceneMap::OnDestroy()
{
	Safe_Release(m_pParentTF);
}

void CSceneMap::SetMapName(const wstring& _name)
{
	m_strName = _name;

	m_pParentTF->Get_GameObject()->Set_ObjectName(m_strMapName);
}

const wstring& CSceneMap::GetMapName()
{
	return m_strName;
}

void CSceneMap::CreateObject(const MapObjectType _type)
{
	_int index = 0;

	switch (_type)
	{
	case MapObjectType::Building:
		index = static_cast<_int>(m_vBuildingList.size());
		break;
	case MapObjectType::Floor:
		index = static_cast<_int>(m_vFloorList.size());
		break;
	case MapObjectType::Prop:
		index = static_cast<_int>(m_vPropList.size());
		break;
	default:
		break;
	}

	wstring typeName = {};

	switch (_type)
	{
	case MapObjectType::Building:
		typeName = L"Building";
		break;
	case MapObjectType::Floor:
		typeName = L"Floor";
		break;
	case MapObjectType::Prop:
		typeName = L"Prop";
		break;
	default:
		break;
	}

	CGameObject* mapObj = m_pGameObject->Get_Scene()->Add_GameObject(typeName + CEngineString::ToW2(index));
	wstring spawnName = L"Stage" + CEngineString::ToW2(m_iMapIndex) + L'_' + typeName + L'_' + CEngineString::ToW2(index);
	CDebug::LogError(L"n: " + spawnName);
	vector<CMeshRenderer*> renders = mapObj->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(spawnName + L" (MeshBuffer)"), 0.01f);

	for (size_t i = 0; i < renders.size(); ++i)
	{
		renders[i]->Get_Material()->Set_FloatValue(L"gRoughness", 0.4f);
		renders[i]->Get_Material()->Set_FloatValue(L"gMetallic", 0.3f);

		CTexture* td = CResources::GetInstance().LoadOnScene<CTexture>(spawnName + L'_' + L"TD" + L'_' + to_wstring(i) + L" (Texture)");
		if (td)
			renders[i]->Get_Material()->Set_Texture(td);
		CTexture* tn = CResources::GetInstance().LoadOnScene<CTexture>(spawnName + L'_' + L"TN" + L'_' + to_wstring(i) + L" (Texture)");
		if (tn)
			renders[i]->Get_Material()->Set_Texture(tn, 1);
	}

	mapObj->GetTransform()->SetParent(m_pParentTF);
	mapObj->GetTransform()->Set_LocalScale(1.f);

	MapObject mapObjStruct = {};

	mapObjStruct.type = _type;
	mapObjStruct.renderers = renders;

	switch (_type)
	{
	case MapObjectType::Building:
		m_vBuildingList.push_back(mapObjStruct);
		break;
	case MapObjectType::Floor:
		m_vFloorList.push_back(mapObjStruct);
		break;
	case MapObjectType::Prop:
		m_vPropList.push_back(mapObjStruct);
		break;
	default:
		break;
	}
}
