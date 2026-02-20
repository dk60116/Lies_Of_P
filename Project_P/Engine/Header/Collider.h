#pragma once
#include "Component.h"
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h> 
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h> 

using namespace JPH;

NS_BEGIN(Engine)

class ENGINE_DLL CCollider abstract : public CComponent
{
	friend class CRigidBody;

public:
	enum class ColliderType { Cube, Sphere, Capsule, Mesh };

protected:
	explicit CCollider();
	~CCollider();

public:
    HRESULT Initialize() override;
    void Awake() override;
    void Update() override;
    void OnDestroy() override;

    const _bool IsTrigger() const;
    const vector3& GetCenter() const;
    void SetTrigger(const _bool isTrigger);
    void SetCenter(const vector3& center);

    const Shape* GetShape();

public:
    virtual void BuildShapeIfNeeded() PURE;
    virtual void ReleaseShape();

protected:
    class CRigidBody* m_pRigidBody;
    _bool m_bIsTrigger;
    vector3 m_vCenter;

    _bool m_bShapeDirty;
    const mutable Shape* m_pShape;
};

NS_END
