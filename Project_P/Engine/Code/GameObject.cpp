#include "epch.h"
#include "GameObject.h"

CGameObject::CGameObject(const wstring _name, ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: m_iUniqueID(999999)
	, m_strGameObjectName(L"")
	, m_bActive(true)
	, m_bPrevActive(true)
	, m_bRecursiveActive(true)
	, m_lComponentList({})
	, m_pScene(nullptr)
	, m_pTransform(nullptr)
	, m_pDevice(_pDevice)
	, m_pContext(_pContext)
	, m_bIsBoneTransform(false)
{
	m_strName = L"Game Object";
	m_pDevice->AddRef();
	m_pContext->AddRef();
}

CGameObject::CGameObject(const CGameObject& _rhs)
	: m_iUniqueID(999999)
	, m_strGameObjectName(_rhs.m_strGameObjectName + L" (Clone)")
	, m_bActive(_rhs.m_bActive)
	, m_bPrevActive(_rhs.m_bPrevActive)
	, m_bRecursiveActive(_rhs.m_bRecursiveActive)
	, m_lComponentList({})
	, m_pScene(_rhs.m_pScene)
	, m_pTransform(nullptr)
	, m_pDevice(_rhs.m_pDevice)
	, m_pContext(_rhs.m_pContext)
	, m_bIsBoneTransform(_rhs.m_bIsBoneTransform)
{
	m_iUniqueID = CSceneManager::GetInstance().Get_CrtScene()->Get_UniqueObjectCount();
}

CGameObject::~CGameObject()
{
	OnDestroy();
}

HRESULT CGameObject::Initialize()
{
	if (!GetComponent<CTransform>())
		m_pTransform = AddComponent<CTransform>();

	return S_OK;
}

void CGameObject::Awake()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Awake();
	}
}

void CGameObject::Start()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Start();
	}
}

void CGameObject::Update_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Update_Editor();
	}
}

void CGameObject::Update()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Update();
	}
}

void CGameObject::FixedUpdate()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->FixedUpdate();
	}
}

void CGameObject::LateUpdate()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->LateUpdate();
	}
}

void CGameObject::LateUpdate_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->LateUpdate_Editor();
	}
}

void CGameObject::OnMouseEnter()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseEnter();
	}
}

void CGameObject::OnMouseOver()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseOver();
	}
}

void CGameObject::OnMouseExit()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseExit();
	}
}

void CGameObject::OnMouseDown()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseDown();
	}
}

void CGameObject::OnMouseDrag()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseDrag();
	}
}

void CGameObject::OnMouseUp()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnMouseUp();
	}
}

void CGameObject::OnPreCull_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPreCull_Editor();
	}
}

void CGameObject::OnPreRender_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPreRender_Editor();
	}
}

void CGameObject::Render_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Render_Editor();
	}
}

void CGameObject::OnPostRender_Editor()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPostRender_Editor();
	}
}

void CGameObject::OnPreCull()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPreCull();
	}
}

void CGameObject::OnPreRender()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPreRender();
	}
}

void CGameObject::Render()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->Render();
	}
}

void CGameObject::OnPostRender()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnPostRender();
	}
}

void CGameObject::Render_Gizmo()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
		(*it)->Render_Gizmo();
}

void CGameObject::OnEnable()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnEnable();
	}
}

void CGameObject::OnDisable()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnDisable();
	}
}

void CGameObject::OnDestroy()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		(*it)->OnDestroy();
		Safe_Release(*it);
	}

	m_lComponentList.clear();
}

void CGameObject::OnApplicationQuit()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if ((*it)->Get_Enable())
			(*it)->OnApplicationQuit();
	}
}

const _bool CGameObject::IsActive() const
{
	return m_bActive;
}

void CGameObject::SetActive(const _bool _active)
{
	m_bActive = _active;
}

list<CComponent*>& CGameObject::Get_ComponentList()
{
	return m_lComponentList;
}

CTransform* CGameObject::Get_Transform() const
{
	return m_pTransform;
}

void CGameObject::Set_Transform(CTransform* _transform)
{
	m_lComponentList.remove(m_pTransform);
	Safe_Release(m_pTransform);

	m_pTransform = nullptr;

	m_pTransform = _transform;

	if (m_pTransform)
	{
		m_lComponentList.push_back(m_pTransform);
		m_pTransform->AddRef();
	}
}

vector<CMeshRenderer*> CGameObject::CreateMeshHierachy(vector<MeshBundle> _meshInfos, const _float _scaleFactor)
{
	CTransform* parentTransform = Get_Transform();

	if (_meshInfos.empty())
	{
		CDebug::LogError(L"Failed create Mesh hierachy - empty mesh info: " + parentTransform->Get_GameObject()->Get_ObjectNameID());
		return {};
	}

	vector<CMeshRenderer*> renderers = {};

	for (_uint i = 0; i < _meshInfos.size(); ++i)
	{
		if (!_meshInfos[i].meshBuffer)
			continue;

		CGameObject* child = m_pScene->Add_GameObject(_meshInfos[i].meshBuffer->Get_ResourceName());
		child->Get_Transform()->SetParent(parentTransform);
		child->Get_Transform()->Set_LocalScale(_scaleFactor);

		CMeshRenderer* ren = child->AddComponent<CMeshRenderer>();

		renderers.push_back(ren);

		ren->Get_MeshFilter()->Set_MeshBuffer(_meshInfos[i].meshBuffer);
		ren->Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)"));

		if (!_meshInfos[i].texture)
			continue;

		ren->Get_Material()->Set_Texture(_meshInfos[i].texture, 0);
	}

	return renderers;
}

vector<CSkinnedMeshRenderer*> CGameObject::CreateSkinnedMeshHierachy(vector<SkinnedMeshBundle> _skinnedInfos, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _bonesInfo, const _float _scaleFactor, const vector3 _rotationFactor)
{
	CTransform* rootTf = Get_Transform();

	if (_skinnedInfos.empty())
	{
		CDebug::LogError(L"Failed create Mesh hierachy - empty skinned info: " + rootTf->Get_GameObject()->Get_ObjectNameID());
		return {};
	}

	vector<CSkinnedMeshRenderer*> renderers = {};
	renderers.reserve(_skinnedInfos.size());

	for (auto& si : _skinnedInfos)
	{
		if (!si.meshBuffer)
			continue;

		CGameObject* g = m_pScene->Add_GameObject(si.meshBuffer->Get_ResourceName());
		g->Get_Transform()->SetParent(rootTf);
		auto* r = g->AddComponent<CSkinnedMeshRenderer>();
		r->Set_MeshBuffer(si.meshBuffer);
		r->Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)"));
		if (si.texture) 
			r->Get_Material()->Set_Texture(si.texture, 0);
		renderers.push_back(r);
	}

	CTransform* rootBone = nullptr;

	vector<CTransform*> boneTfs(_bonesInfo.size(), nullptr);
	for (size_t i = 0; i < _bonesInfo.size(); ++i)
	{
		CGameObject* g = m_pScene->Add_GameObject(_bonesInfo[i].name);
		boneTfs[i] = g->Get_Transform();
		g->m_bIsBoneTransform = true;

		if (_bonesInfo[i].parentId == -1)
			rootBone = g->Get_Transform()->Get_Transform();
	}

	for (size_t i = 0; i < _bonesInfo.size(); ++i)
	{
		_int pid = _bonesInfo[i].parentId;
		boneTfs[i]->SetParent(pid >= 0 ? boneTfs[pid] : rootTf);
	}

	for (size_t i = 0; i < _bonesInfo.size(); ++i)
	{
		_matrix m = XMLoadFloat4x4(&_bonesInfo[i].transformation);
		_vector S, R, T;
		XMMatrixDecompose(&S, &R, &T, m);
		boneTfs[i]->Set_LocalScale(S);
		boneTfs[i]->Set_LocalQuaternion(R);
		boneTfs[i]->Set_LocalPosition(T);
	}

	rootBone->Set_LocalScale(_scaleFactor);
	rootBone->Set_LocalEulerAngles(_rotationFactor);

	unordered_map<wstring, CTransform*> nameMap;
	nameMap.reserve(_bonesInfo.size());
	for (size_t i = 0; i < _bonesInfo.size(); ++i)
		nameMap[_bonesInfo[i].name] = boneTfs[i];

	CTransform* baseTransform = nullptr;

	for (auto& b : _bonesInfo)
		if (b.parentId == -1) 
		{ 
			baseTransform = nameMap[b.name];
			break; 
		}

	for (auto* r : renderers)
	{
		_uint bc = r->Get_SkinnedMeshBuffer()->Get_BoneCount();
		vector<CTransform*> bones(bc, nullptr);
		for (_uint i = 0; i < bc; ++i)
		{
			const wstring& bn = r->Get_SkinnedMeshBuffer()->Get_BoneName(i);
			auto it = nameMap.find(bn);
			if (it != nameMap.end())
				bones[i] = it->second;
		}

		r->Set_Bones(bones, baseTransform);
	}

	return renderers;
}

const _uint CGameObject::Get_UniqueID() const
{
	return m_iUniqueID;
}

const wstring CGameObject::Get_ObjectName() const
{
	return m_strGameObjectName;
}

const wstring CGameObject::Get_ObjectNameID() const
{
	return m_strGameObjectName + L"[" + to_wstring(m_iUniqueID) + L"]";
}

void CGameObject::Set_ObjectName(wstring& _name)
{
	m_strGameObjectName = _name;
}

void CGameObject::Set_Scene(CScene* _scene)
{
	m_pScene = _scene;
}

CScene* CGameObject::Get_Scene()
{
	return m_pScene;
}

const _bool CGameObject::IsBoneTransform() const
{
	return m_bIsBoneTransform;
}

CGameObject* CGameObject::Instantiate(const CGameObject* _rhs)
{
	CGameObject* newGameObj = CSceneManager::GetInstance().Get_CrtScene()->Add_GameObject(_rhs->Get_ObjectName() + L" (Clone)");

	for (TRAVERSAL_ITER(_rhs->m_lComponentList, it))
	{
		if (*it && !dynamic_cast<CTransform*>(*it))
		{
			newGameObj->m_lComponentList.push_back((*it)->Clone());
			newGameObj->m_lComponentList.back()->Set_Object(newGameObj);
			newGameObj->m_lComponentList.back()->AddRef();

			newGameObj->m_lComponentList.back()->Initialize();
		}
	}

	newGameObj->Get_Transform()->SetTransformForMatrix(_rhs->Get_Transform()->Get_WorldMatrix());

	return newGameObj;
}

const _bool CGameObject::IsRecursiveActive()
{
	return m_bRecursiveActive;
}

void CGameObject::Set_RecursiveActive(const _bool _active)
{
	m_bRecursiveActive = _active;

	if (m_pTransform)
	{
		const auto& children = m_pTransform->Get_ChldList();
		
		for (auto child : children)
		{
			if (child && child->Get_GameObject())
				child->Get_GameObject()->Set_RecursiveActive(_active);
		}
	}
}
