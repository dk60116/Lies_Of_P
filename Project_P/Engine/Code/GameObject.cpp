#include "epch.h"
#include "GameObject.h"
#include <objbase.h>

namespace
{
	wstring GenerateGuidString()
	{
		GUID guid = {};
		if (FAILED(CoCreateGuid(&guid)))
			return L"";

		wchar_t buffer[39] = {};
		if (StringFromGUID2(guid, buffer, 39) == 0)
			return L"";

		return wstring(buffer);
	}
}


CGameObject::CGameObject(const wstring _name, ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: m_iUniqueID(999999)
	, m_strGuid(GenerateGuidString())
	, m_strGameObjectName(L"")
	, m_bActive(true)
	, m_bActive_Origin(true)
	, m_bPrevActive(true)
	, m_bRecursiveActive(true)
	, m_lComponentList({})
	, m_pScene(nullptr)
	, m_pTransform(nullptr)
	, m_pDevice(_pDevice)
	, m_pContext(_pContext)
	, m_bIsBoneTransform(false)
	, m_bSaveTarget(true)
	, m_bKill(false)
	, m_bTransformStatic(false)
{
	m_strName = L"Game Object";
	m_pDevice->AddRef();
	m_pContext->AddRef();
}

CGameObject::CGameObject(const CGameObject& _rhs)
	: m_iUniqueID(999999)
	, m_strGuid(GenerateGuidString())
	, m_strGameObjectName(_rhs.m_strGameObjectName + L" (Clone)")
	, m_bActive(_rhs.m_bActive)
	, m_bActive_Origin(_rhs.m_bActive_Origin)
	, m_bPrevActive(_rhs.m_bPrevActive)
	, m_bRecursiveActive(_rhs.m_bRecursiveActive)
	, m_lComponentList({})
	, m_pScene(_rhs.m_pScene)
	, m_pTransform(nullptr)
	, m_pDevice(_rhs.m_pDevice)
	, m_pContext(_rhs.m_pContext)
	, m_bIsBoneTransform(_rhs.m_bIsBoneTransform)
	, m_bSaveTarget(_rhs.m_bSaveTarget)
	, m_bKill(false)
	, m_bTransformStatic(_rhs.m_bTransformStatic)
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

void CGameObject::Destroy()
{
	m_bKill = true;
}

_bool CGameObject::RemoveComponent(CComponent* _component)
{
	if (!_component)
		return false;

	auto it = find(m_lComponentList.begin(), m_lComponentList.end(), _component);
	if (it == m_lComponentList.end())
		return false;

	if (dynamic_cast<CTransform*>(_component) || dynamic_cast<CRectTransform*>(_component))
		return false;

	if (CCamera* cam = dynamic_cast<CCamera*>(_component))
	{
		if (m_pScene)
			m_pScene->Remove_Camera(cam);
	}

	if (CLight* light = dynamic_cast<CLight*>(_component))
	{
		if (m_pScene)
			m_pScene->Remove_Light(light);
	}

	if (CCanvas* canvas = dynamic_cast<CCanvas*>(_component))
	{
		if (m_pScene)
			m_pScene->Remove_Canvas(canvas);
	}

	_component->OnDestroy();
	m_lComponentList.erase(it);
	Safe_Release(_component);

	return true;
}

const _bool CGameObject::IsActive() const
{
	return m_bActive;
}

const _bool CGameObject::IsActive_Origin() const
{
	return m_bActive_Origin;
}

void CGameObject::SetActive(const _bool _active)
{
	m_bActive = _active;
	m_bActive_Origin = _active;
	m_bPrevActive = _active;

	_bool parentActive = true;
	if (m_pTransform)
	{
		if (CTransform* parent = m_pTransform->Get_Parent())
		{
			if (parent->Get_GameObject())
				parentActive = parent->Get_GameObject()->IsRecursiveActive();
		}
	}

	Set_RecursiveActive(parentActive && m_bActive);
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

	if (!m_pScene || !parentTransform)
	{
		CDebug::LogError(L"CreateMeshHierachy failed - scene/transform null: " + Get_ObjectNameID());
		return {};
	}

	if (_meshInfos.empty())
	{
		CDebug::LogError(L"Failed create Mesh hierachy - empty mesh info: " +
			parentTransform->Get_GameObject()->Get_ObjectNameID());
		return {};
	}

	vector<CMeshRenderer*> renderers;
	renderers.reserve(_meshInfos.size());

	_uint childIndex = 0;

	for (const auto& mb : _meshInfos)
	{
		if (!mb.meshBuffer)
			continue;

		wstring childName = mb.meshBuffer->Get_ResourceName() + L"_" + to_wstring(childIndex++);
		CGameObject* child = m_pScene->Add_GameObject(childName);
		if (!child || !child->Get_Transform())
			continue;

		child->Get_Transform()->SetParent(parentTransform);
		child->Get_Transform()->Set_LocalPosition(vector3::zero());
		child->Get_Transform()->Set_LocalEulerAngles(vector3::zero());
		child->Get_Transform()->Set_LocalScale(_scaleFactor);

		CMeshRenderer* ren = child->AddComponent<CMeshRenderer>();

		if (!ren || !ren->Get_MeshFilter())
			continue;

		renderers.push_back(ren);

		ren->Get_MeshFilter()->Set_MeshBuffer(mb.meshBuffer);

		CMaterial* mat = CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)");
		ren->Set_Material(mat);

		if (mb.texture && ren->Get_Material())
			ren->Get_Material()->Set_Texture(mb.texture, 0);
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
		r->Get_MeshBuffer()->Set_Scalefactor(_scaleFactor);
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
			rootBone = g->Get_Transform();
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
		if (!XMMatrixDecompose(&S, &R, &T, m))
		{
			const _float4x4& M = _bonesInfo[i].transformation;
			T = XMVectorSet(M._41, M._42, M._43, 0.f);
			R = XMQuaternionIdentity();
			S = XMVectorSet(1.f, 1.f, 1.f, 0.f);
		}
		boneTfs[i]->Set_LocalScale(S);
		boneTfs[i]->Set_LocalQuaternion(R);
		boneTfs[i]->Set_LocalPosition(T);
	}

	if (!rootBone)
		rootBone = rootTf;

	rootBone->Set_LocalScale(_scaleFactor);
	rootBone->Set_LocalEulerAngles(_rotationFactor);

	unordered_map<wstring, CTransform*> nameMap;
	nameMap.reserve(_bonesInfo.size());
	for (size_t i = 0; i < _bonesInfo.size(); ++i)
		nameMap[_bonesInfo[i].name] = boneTfs[i];

	vector<CTransform*> baseTransforms = {};

	for (auto& b : _bonesInfo)
	{
		if (b.parentId == -1)
			baseTransforms.push_back(nameMap[b.name]);
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

		r->Set_Bones(bones, baseTransforms);
	}

	return renderers;
}

const _uint CGameObject::Get_UniqueID() const
{
	return m_iUniqueID;
}

const wstring& CGameObject::Get_Guid() const
{
	return m_strGuid;
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

const _bool CGameObject::Is_SaveTarget() const
{
	return m_bSaveTarget;
}

const _bool CGameObject::IsStatic(STATIC_METHOD _method) const
{
	switch (_method)
	{
	case STATIC_METHOD::TransformStatic:
		return m_bTransformStatic;
	}

	return false;
}

void CGameObject::SetStatic(STATIC_METHOD _method, const _bool _value, const _bool recursiveChild)
{
	switch (_method)
	{
	case STATIC_METHOD::TransformStatic:
		m_bTransformStatic = _value;
		break;
	}

	if (!recursiveChild || !m_pTransform)
		return;

	for (CTransform* child : m_pTransform->Get_ChldList())
	{
		if (!child)
			continue;

		if (CGameObject* childObj = child->Get_GameObject())
			childObj->SetStatic(_method, _value, true);
	}
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
			{
				CGameObject* childObj = child->Get_GameObject();
				childObj->Set_RecursiveActive(_active && childObj->m_bActive);
			}
		}
	}
}
