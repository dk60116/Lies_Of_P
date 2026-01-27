#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CTransform : public CComponent
{
	friend class CGameObject;

public:
	typedef struct TransformDirections
	{
		vector3 forward = vector3::zero();
		vector3 back = vector3::zero();
		vector3 left = vector3::zero();
		vector3 right = vector3::zero();
		vector3 up = vector3::zero();
		vector3 down = vector3::zero();
	}DIRECTIONS;

	enum LookAtFilter { X = 0x001, Y = 0x010, Z = 0x100 };

protected:
	explicit CTransform();
	~CTransform();

private:
	static CTransform* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void LateUpdate() override;
	void Render_Gizmo() override;
	void OnDestroy() override;

public:
	CTransform* Get_Parent() const;
	virtual void SetParent(CTransform* _parent);
	const _bool Is_Root() const;
	CTransform* Get_Child();
	CTransform* Get_Child(const _int _index);
	CTransform* Find_Child(wstring _name);
	CTransform* Find_ChildRecursive(wstring _name);
	const list<CTransform*>& Get_ChldList() const;
	const DIRECTIONS& Get_Directions();
	const _matrix Get_WorldMatrix() const;
	const _matrix Get_LocalMatrix() const;
	const _matrix Get_InverseWorldMatrix() const;

public:
	template <typename T>
	T* Find_ComponentParentRecursive();

public:
	const vector3& Get_Position();
	const vector3& Get_LocalPosition();

	const vector3 Get_EulerAngles();
	const vector3 Get_LocalEulerAngles();

	vector3& Get_LocalScale();

	const quaternion Get_Quaternion() const;
	const quaternion& Get_LocalQuaternion() const;

public:
	void Set_Position(const vector3& _pos);
	void Set_Position(const _float _x, const _float _y, const _float _z);
	void Set_PositionX(const _float _value);
	void Set_PositionY(const _float _value);
	void Set_PositionZ(const _float _value);

	void Set_LocalPosition(const vector3& _pos);
	void Set_LocalPosition(const _float _x, const _float _y, const _float _z);
	void Set_LocalPositionX(const _float _value);
	void Set_LocalPositionY(const _float _value);
	void Set_LocalPositionZ(const _float _value);

	void Add_Position(const vector3& _value);
	void Add_Position(const _float _x, const _float _y, const _float _z);
	void Add_PositionX(const _float _value);
	void Add_PositionY(const _float _value);
	void Add_PositionZ(const _float _value);

	void Add_LocalPosition(const vector3& _value);
	void Add_LocalPosition(const _float _x, const _float _y, const _float _z);
	void Add_LocalPositionX(const _float _value);
	void Add_LocalPositionY(const _float _value);
	void Add_LocalPositionZ(const _float _value);

	void Set_Quaternion(const quaternion& _value);
	void Set_LocalQuaternion(const quaternion& _value);

	void Add_Quaternion(const quaternion& _delta);

	void Set_EulerAngles(const vector3& _rot);
	void Set_EulerAngles(const _float _x, const _float _y, const _float _z);
	void Set_EulerAnglesX(const _float _x);
	void Set_EulerAnglesY(const _float _y);
	void Set_EulerAnglesZ(const _float _z);

	void Add_EulerAngles(const vector3& _rot);
	void Add_EulerAngles(const _float _x, const _float _y, const _float _z);
	void Add_EulerAnglesX(const _float _value);
	void Add_EulerAnglesY(const _float _value);
	void Add_EulerAnglesZ(const _float _value);

	void Set_LocalEulerAngles(const vector3& _rot);
	void Set_LocalEulerAngles(const _float _x, const _float _y, const _float _z);
	void Set_LocalEulerAnglesX(const _float _x);
	void Set_LocalEulerAnglesY(const _float _y);
	void Set_LocalEulerAnglesZ(const _float _z);

	void Add_LocalEulerAngles(const vector3& _rot);
	void Add_LocalEulerAngles(const _float _x, const _float _y, const _float _z);
	void Add_LocalEulerAnglesX(const _float _value);
	void Add_LocalEulerAnglesY(const _float _value);
	void Add_LocalEulerAnglesZ(const _float _value);

	void Set_LocalScale(const vector3& _scale);
	void Set_LocalScale(const _float _x, const _float _y, const _float _z);
	void Set_LocalScale(const _float _value);
	void Set_LocalScaleX(const _float _value);
	void Set_LocalScaleY(const _float _value);
	void Set_LocalScaleZ(const _float _value);

	void Add_LocalScale(const vector3& _scale);
	void Add_LocalScaleX(const _float _value);
	void Add_LocalScaleY(const _float _value);
	void Add_LocalScaleZ(const _float _value);

	const vector3& Get_PrevPosition();
	const vector3& Get_PrevLocalPos();
	const vector3& Get_PrevEulerAngles();
	const vector3& Get_PrevLocalEuler();
	const quaternion& Get_PrevQuaternion();
	const quaternion& Get_PrevLocalQuat();

	void SetTransformForMatrix(_matrix _matWorld);

	void LookAt(const vector3& _target, const _uint _lockRotationFilter = 0x000);
	const vector3 LookRotation(const vector3& _target, const _uint _lockRotationFilter = 0x000);
	const quaternion LookQuaternion(const vector3& _target, const _uint _lockRotationFilter = 0x000);

private:
	void Bind_Matrix();
	void Bind_Direction();
	static void RecalcWorldUpChain(CTransform* _t);

protected:
	_bool m_bIsRootParent;
	CTransform* m_pParent;
	list<CTransform*> m_lChildList;
	vector3 m_vPosition, m_vEulerAngles, m_vScale;
	vector3 m_vWorldPosition, m_vWorldEulerAngles;
	quaternion m_vQuaternion, m_vWorldQuaternion;
	quaternion m_vPrevQuaternion, m_vPrevLocalQuat;
	_float4x4 m_vMatWorld, m_vMatLocal, m_vMatLocalRotation;
	vector3 m_vPrevPosition, m_vPrevEulerAngles;
	vector3 m_vPrevLoclaPos, m_vPrevLocalEuler, m_vPrevLocalScale;
	DIRECTIONS m_sDirections, m_sPrevDirections;
};

NS_END

template<typename T>
inline T* CTransform::Find_ComponentParentRecursive()
{
	CTransform* tempParent = this;

	while (tempParent)
	{
		if (T* com = tempParent->m_pGameObject->GetComponent<T>())
			return com;

		tempParent = tempParent->m_pParent;
	}

	return nullptr;
}
