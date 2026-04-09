#pragma once
#include "Component.h"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>

using namespace JPH;

NS_BEGIN(Engine)

class ENGINE_DLL CRigidBody final : public CComponent
{
	friend class CGameObject;

protected:
	explicit CRigidBody();
	~CRigidBody();

private:
	static CRigidBody* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void FixedUpdate() override;
	void OnCollisionEnter(CCollider* _other) override;
	void OnCollisionStay(CCollider* _other) override;
	void OnCollisionExit(CCollider* other) override;
	void OnTriggerEnter(CCollider* other) override;
	void OnTriggerStay(CCollider* _other) override;
	void OnTriggerExit(CCollider* _other) override;
	void OnDestroy() override;

public:
	static void BuildCompoundShapes(const list<CCollider*>& _colliders, RefConst<Shape>& _outBodyCompound, RefConst<Shape>& _outSensorCompound);

public:
	const BodyID GetBodyID() const;
	const BodyID GetSensorBodyID() const;

public:
	void AddCollider(CCollider* _collider);
	void RemvoeCollier(CCollider* _collider);

public:
	void MarkBodyDirty();
	_bool IsKinematic() const;
	void SetKinematic(_bool _kinematic);
	_bool IsUseGravity() const;
	void SetUseGravity(_bool _useGravity);
	_float GetMass() const;
	void SetMass(_float _mass);
	_float GetDrag() const;
	void SetDrag(_float _drag);
	_float GetAngularDrag() const;
	void SetAngularDrag(_float _angularDrag);
	_bool IsConstPositionX() const;
	void SetConstPositionX(_bool _value);
	_bool IsConstPositionY() const;
	void SetConstPositionY(_bool _value);
	_bool IsConstPositionZ() const;
	void SetConstPositionZ(_bool _value);
	_bool IsConstRotationX() const;
	void SetConstRotationX(_bool _value);
	_bool IsConstRotationY() const;
	void SetConstRotationY(_bool _value);
	_bool IsConstRotationZ() const;
	void SetConstRotationZ(_bool _value);
	void Translate(const vector3& _deltaWorld);
	void Rotate(const vector3& _deltaEuler);
	void SetVelocity(const vector3& _value);
	void SetVelocityX(const _float _value);
	void SetVelocityY(const _float _value);
	void SetVelocityZ(const _float _value);
	void ResetVelocity();
	void SuspendLinearDragUntilNextPhysicsStep();
	void AddForce(const vector3& _force);
	void AddForceX(_float _force);
	void AddForceY(_float _force);
	void AddForceZ(_float _force);
	CCollider* GetEventCollider(_bool _triggerEvent) const;

private:
	void RebuildBodiesIfDirty();
	void DestroyBodies();

	void SyncKinematicToJolt();
	void SyncDynamicFromJolt();
	void ApplyPositionConstraints(vector3& _pos);
	void ApplyRotationConstraints(quaternion& _rot);
	void ApplyAxisConstraints(vector3& _pos, quaternion& _rot);
	void SyncPositionConstraintCache(const vector3& _pos);
	void CacheLastSyncedTransform(const vector3& _pos, const quaternion& _rot);

private:
	list<CCollider*> m_lColliderList;

	_bool m_bBodyDirty;

	BodyID m_iBodyID;
	_bool m_bHasBody;

	BodyID m_iSensorBodyID;
	_bool m_bHasSensorBody;

	const Shape* m_pCompoundShape;
	const Shape* m_pSensorCompoundShape;

	_bool m_bKinematic;
	_bool m_bUseGravity;
	_float m_fMass;
	_float m_fDrag;
	_float m_fAngularDrag;

	_bool m_bConstPositionX;
	_bool m_bConstPositionY;
	_bool m_bConstPositionZ;
	_bool m_bConstRotationX;
	_bool m_bConstRotationY;
	_bool m_bConstRotationZ;
	vector3 m_vConstPosition;
	vector3 m_vConstRotation;
	_bool m_bSkipPositionConstraintSyncOnce;
	_bool m_bRestoreDragAfterTranslate;

	_bool m_bHasLastSyncedTransform;
	vector3 m_vLastSyncedPosition;
	quaternion m_vLastSyncedRotation;
};

NS_END
