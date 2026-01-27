#pragma once

#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL CComponent abstract : public UObject
{
	friend class CGameObject;

protected:
	CComponent();
	~CComponent();

protected:
	virtual CComponent* Clone() const PURE;

public:
	virtual HRESULT Initialize();
	void ComponentRelease();

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
	virtual void OnDestroy() PURE;

	virtual void OnApplicationQuit();

public:
	const _bool Get_Enable() const;
	void Set_Enable(const _bool _enable);

	CGameObject* Get_GameObject();
	class CTransform* Get_Transform();

private:
	void Set_Object(class CGameObject* _gameObject);

protected:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

protected:
	_bool m_bEnable;
	class CGameObject* m_pGameObject;

public:
	virtual vector<FieldInfo> GetInspectorFields();

protected:
	inline FieldType DetectFieldType(bool&) { return FieldType::BOOL; }
	inline FieldType DetectFieldType(int&) { return FieldType::INT; }
	inline FieldType DetectFieldType(float&) { return FieldType::FLOAT; }
	inline FieldType DetectFieldType(vector2&) { return FieldType::VECTOR2; }
	inline FieldType DetectFieldType(vector2Int&) { return FieldType::VECTOR2INT; }
	inline FieldType DetectFieldType(vector3&) { return FieldType::VECTOR3; }
	inline FieldType DetectFieldType(vector3Int&) { return FieldType::VECTOR3INT; }
	inline FieldType DetectFieldType(wstring&) { return FieldType::STRING; }
	inline FieldType DetectFieldType(UObject*) { return FieldType::POINTER; }
	inline FieldType DetectFieldType(ColorValue&) { return FieldType::COLOR; }
};

NS_END

