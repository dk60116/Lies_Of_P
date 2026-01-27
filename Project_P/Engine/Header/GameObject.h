#pragma once

#include "Object.h"
#include "Component.h"
#include "Scene.h"
#include "EditorCamera.h"
#include "SkinnedMeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CGameObject final : public UObject
{
	friend class CSceneManager;
	friend class CScene;
	friend class CTransform;

private:
	explicit CGameObject(const wstring _name, ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
	explicit CGameObject(const CGameObject& _rhs);
	virtual ~CGameObject();

public:
	HRESULT Initialize();

public:
	virtual void Awake();
	virtual void Start();
	virtual void Update_Editor();
	virtual void Update();
	virtual void FixedUpdate();
	virtual void LateUpdate_Editor();
	virtual void LateUpdate();

	virtual void OnMouseEnter();
	virtual void OnMouseOver();
	virtual void OnMouseExit();
	virtual void OnMouseDown();
	virtual void OnMouseDrag();
	virtual void OnMouseUp();

	//virtual void OnCollisionEnter();
	//virtual void OnCollisionStay();
	//virtual void OnCollisionExit();
	//virtual void OnTriggerEnter();
	//virtual void OnTriggerStay();
	//virtual void OnTriggerExit();

	//virtual void OnCollisionEnter2D();
	//virtual void OnCollisionStay2D();
	//virtual void OnCollisionExit2D();
	//virtual void OnTriggerEnter2D();
	//virtual void OnTriggerStay2D();
	//virtual void OnTriggerExit2D();

	virtual void OnPreCull_Editor();
	virtual void OnPreRender_Editor();
	virtual void Render_Editor();
	virtual void OnPostRender_Editor();

	virtual void OnPreCull();
	virtual void OnPreRender();
	virtual void Render();
	virtual void OnPostRender();

	virtual void Render_Gizmo();

	virtual void OnEnable();
	virtual void OnDisable();
	virtual void OnDestroy();

	virtual void OnApplicationQuit();

public:
	template<typename T>
	T* AddComponent();
	template<typename T>
	T* GetComponent();

	const _bool IsActive() const;
	void SetActive(const _bool _active);

	list<CComponent*>& Get_ComponentList();
	class CTransform* Get_Transform() const;
	void Set_Transform(CTransform* _transform);

	vector<class CMeshRenderer*> CreateMeshHierachy(vector<MeshBundle> _meshInfos, const _float _scaleFactor = 1.f);
	vector<class CSkinnedMeshRenderer*> CreateSkinnedMeshHierachy(vector<SkinnedMeshBundle> _skinnedInfos, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _bonesInfo, const _float _scaleFactor = 1.f, const vector3 _rotationFactor = vector3::zero());

	const _uint Get_UniqueID() const;
	const wstring Get_ObjectName() const;
	const wstring Get_ObjectNameID() const;
	void Set_ObjectName(wstring& _name);

	void Set_Scene(CScene* _scene);
	CScene* Get_Scene();

	const _bool IsBoneTransform() const;

public:
	static CGameObject* Instantiate(const CGameObject* _rhs);

	template<typename T>
	static T* FindObjectOfType();

	template<typename T>
	static vector<T*> FindObjectsOfType();

	const _bool IsRecursiveActive();

private:
	void Set_RecursiveActive(const _bool _active);

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

private:
	_uint m_iUniqueID;
	wstring m_strGameObjectName;
	_bool m_bActive, m_bPrevActive, m_bRecursiveActive;

	list<CComponent*> m_lComponentList;

	CScene* m_pScene;
	class CTransform* m_pTransform;

	_bool m_bIsBoneTransform;
};

NS_END

template<typename T>
inline T* CGameObject::AddComponent()
{
	T* newComponent = T::Create();

	dynamic_cast<CComponent*>(newComponent)->Set_Object(this);
	newComponent->AddRef();
	m_lComponentList.push_back(newComponent);

	if (FAILED(newComponent->Initialize()))
	{
		Safe_Release(newComponent);
		return nullptr;
	}

	if (CCamera* cam = dynamic_cast<class CCamera*>(newComponent))
	{
		if (cam)
			m_pScene->Add_Camera(cam);
	}

	if (CLight* light = dynamic_cast<class CLight*>(newComponent))
	{
		if (light)
			m_pScene->Add_Light(light);
	}

	if (CCanvas* canv = dynamic_cast<class CCanvas*>(newComponent))
	{
		if (canv)
			m_pScene->Add_Canvas(canv);
	}

	return newComponent;
}

template<typename T>
inline T* CGameObject::GetComponent()
{
	for (TRAVERSAL_ITER(m_lComponentList, it))
	{
		if (T* component = dynamic_cast<T*>(*it))
			return component;
	}

	return nullptr;
}

template<typename T>
inline T* CGameObject::FindObjectOfType()
{
	for (CGameObject* obj : CSceneManager::GetInstance().Get_CrtScene()->Get_ObjectList())
	{
		if (T* t = obj->GetComponent<T>())
			return t;
	}
	return nullptr;
}

template<typename T>
inline vector<T*> CGameObject::FindObjectsOfType()
{
	vector<T*> result;

	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();

	for (CGameObject* obj : scene->Get_ObjectList())
	{
		if (T* t = obj->GetComponent<T>())
			result.push_back(t);
	}

	return result;
}
