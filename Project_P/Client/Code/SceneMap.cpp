#include "cpch.h"
#include "SceneMap.h"

CSceneMap::CSceneMap()
	: m_iMapIndex(0)
	, m_strMapName({})
	, m_pParentTF(nullptr)
	, m_vObjectList({})
{
}

CSceneMap::~CSceneMap()
{
}

HRESULT CSceneMap::Initialize()
{
	CScene* scene = m_pGameObject->Get_Scene();

	CGameObject* parentObj = scene->Add_GameObject(m_strMapName);
	m_pParentTF = parentObj->Get_Transform();

	m_pParentTF->SetParent(Get_Transform());

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
	_int index = static_cast<_int>(m_vObjectList.size());

	wstring typeName = {};

	switch (_type)
	{
	case MapObjectType::Building:
		typeName = L"Building";
		break;
	case MapObjectType::Prop:
		typeName = L"Prop";
		break;
	default:
		break;
	}

	CGameObject* mapObj = m_pGameObject->Get_Scene()->Add_GameObject(typeName + CEngineString::ToW2(index));
	wstring spawnName = L"Stage" + CEngineString::ToW2(m_iMapIndex) + L'_' + typeName + L'_' + CEngineString::ToW2(index) + L" (MeshBuffer)";
	CDebug::LogError(L"n: " + spawnName);
	vector<CMeshRenderer*> renders = mapObj->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(spawnName), 0.01f);

	mapObj->Get_Transform()->SetParent(m_pParentTF);
	mapObj->Get_Transform()->Set_LocalScale(1.f);

	MapObject mapObjStruct = {};

	mapObjStruct.type = _type;
	mapObjStruct.renderers = renders;

	m_vObjectList.push_back(mapObjStruct);
}
