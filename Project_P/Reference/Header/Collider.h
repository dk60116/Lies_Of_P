#pragma once
#include "Component.h"
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h> 
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h> 
#include <Jolt/Physics/Body/BodyID.h>

using namespace JPH;

NS_BEGIN(Engine)

class ENGINE_DLL CCollider abstract : public CComponent
{
public:
	enum class ColliderType { Box, Sphere, Capsule, Mesh };

protected:
	explicit CCollider();
	~CCollider();

public:
    HRESULT Initialize() override;
    void Awake() override;
    void OnEnable() override;
    void OnDisable() override;
    void Update() override;
    void OnDestroy() override;

    const _bool IsTrigger() const;
    const vector3& GetCenter() const;
    void SetTrigger(const _bool isTrigger);
    void SetCenter(const vector3& center);

    const Shape* GetShape();

    const _bool IsContacting() const;
    void BeginContact();
    void EndContact();

    void SetRigidBody(class CRigidBody* rigidBody);

public:
    virtual void BuildShapeIfNeeded() PURE;
    virtual void ReleaseShape();

private:
    void RefreshStandaloneBody();
    void CreateStandaloneBody();
    void DestroyStandaloneBody();
    void SyncStandaloneBodyTransform();

protected:
    void NotifyShapeChanged();
    _float4 GetGizmoColor() const;

protected:
    class CRigidBody* m_pRigidBody;
    _bool m_bIsTrigger;
    vector3 m_vCenter;
    vector3 m_vCachedScale;

    _bool m_bShapeDirty;
    const mutable Shape* m_pShape;
    _int m_iContactCount;

    BodyID m_iStandaloneBodyID;
    _bool m_bHasStandaloneBody;
    _bool m_bStandaloneBodyDirty;
    vector3 m_vCachedWorldPosition;
    quaternion m_vCachedWorldRotation;
    _bool m_bDestroying;
};

NS_END
