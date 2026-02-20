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
	_float GetMass() const;
	void SetMass(_float _mass);
	CCollider* GetEventCollider(_bool _triggerEvent) const;

private:
	void RebuildBodiesIfDirty();
	void DestroyBodies();

	void SyncKinematicToJolt();
	void SyncDynamicFromJolt();

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
	_float m_fMass;
};

NS_END
