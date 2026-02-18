#pragma once
#include "Component.h"
#include <unordered_map>

using namespace JPH;

NS_BEGIN(Engine)

class ENGINE_DLL CRigidBody final : public CComponent
{
protected:
	explicit CRigidBody();
	~CRigidBody();

public:
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
	const BodyID GetBodyID() const;
	const BodyID GetSensorBodyID() const;

	void MarkBodyDirty();

private:
	void RebuildBodiesIfNeeded();
	void DestroyBodies();

	const Shape* BuildCompoundShape(bool trigger_only);

public:
	void AddCollider(CCollider* _collider);
	void RemvoeCollier(CCollider* _collider);

private:
	void UpdateContactState(const _bool entering);
	void ClearContactState();

private:
	list<CCollider*> m_lColliderList;
	unordered_map<class CCollider*, _int> m_ColliderContactRefCounts;
	_int m_iContactPairCount;

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

