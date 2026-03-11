#pragma once

#include "epch.h"
#include <array>
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>

using namespace JPH;

NS_BEGIN(Engine)

namespace Layers
{
	static constexpr ObjectLayer NON_MOVING = 0;
	static constexpr ObjectLayer MOVING = 1;
	static constexpr ObjectLayer SENSOR = 2;
	static constexpr ObjectLayer NUM_TYPES = 3;
	static constexpr _uint NUM_SCENE_LAYERS = 32;
	static constexpr _uint NUM_LAYERS = NUM_TYPES * NUM_SCENE_LAYERS;
};

namespace BroadPhaseLayers
{
	static constexpr BroadPhaseLayer NON_MOVING(0);
	static constexpr BroadPhaseLayer MOVING(1);
	static constexpr _uint NUM_BP_LAYERS = 2;
};

class ENGINE_DLL CPhysics final
{
public:
	static constexpr _uint MAX_SCENE_LAYERS = Layers::NUM_SCENE_LAYERS;
	using LayerCollisionMask = _uint;
	using LayerCollisionMaskArray = array<LayerCollisionMask, MAX_SCENE_LAYERS>;

	enum class CollisionObjectType : ObjectLayer
	{
		NonMoving = Layers::NON_MOVING,
		Moving = Layers::MOVING,
		Sensor = Layers::SENSOR
	};

	struct Ray 
	{ 
		vector3 origin; 
		vector3 dir; 
		_float maxDist = 999999.f;
	};

	struct BoxRay
	{
		vector3 center;
		vector3 halfExtent;
		quaternion rotation = quaternion::identity();
		vector3 dir;
		_float maxDist = 999999.f;
	};

	struct SphereRay
	{
		vector3 center;
		_float radius = 0.5f;
		vector3 dir;
		_float maxDist = 999999.f;
	};

	typedef struct RaycastHitInformation
	{
		_bool isHit = false;
		vector3 hitPos = {};
		vector3 hitNormal = {};
		_float distance = 0.f;
		CGameObject* object = nullptr;
	}RAYCASTHIT;

	SINGLETONCLASS(CPhysics);

public:
	HRESULT Initialize();
	void Release();

public:
	void Tick(_float _deltaSeconds);
	void Step(const _float _fixedDeltaSeconds);
	void RenderRaycastDebugDisplay();
	void ClearRaycastDebugDisplay();

public:
	void  SetFixedDeltaTime(const _float _fixedDt);  
	_float GetFixedDeltaTime() const;

	void  SetMaxSubSteps(const _uint _maxSubSteps); 
	void  ResetStepper();

public:
	PhysicsSystem& GetPhysicsSystem();
	void RemoveContactPairs(const BodyID& _bodyID);

	const _bool IsInitialized() const;

	const vector3 GetGravity() const;
	void SetGravity(const vector3& gravity);
	const LayerCollisionMaskArray& GetLayerCollisionMasks() const;
	void SetLayerCollisionMasks(const LayerCollisionMaskArray& masks);
	const _bool GetLayerCollisionEnabled(const _uint layerAIndex, const _uint layerBIndex) const;
	void SetLayerCollisionEnabled(const _uint layerAIndex, const _uint layerBIndex, const _bool enabled);

	static _uint LayerMaskToIndex(const _uint sceneLayerMask);
	static ObjectLayer MakeObjectLayer(const _uint sceneLayerMask, const CollisionObjectType type);
	static CollisionObjectType GetCollisionObjectType(const ObjectLayer layer);
	static _uint GetSceneLayerIndex(const ObjectLayer layer);

	vector<RAYCASTHIT> Raycast(const Ray& _ray, const CSceneManager::LayerMask _mask = 0);
	vector<RAYCASTHIT> BoxRaycast(const BoxRay& _boxRay, const CSceneManager::LayerMask _mask = 0);
	vector<RAYCASTHIT> SphereRaycast(const SphereRay& _sphereRay, const CSceneManager::LayerMask _mask = 0);

private:
	class BroadPhaseLayerInterfaceImpl;
	class ObjectVsBroadPhaseLayerFilterImpl;
	class ObjectLayerPairFilterImpl;
	class ContactListenerImpl;

	_bool m_bJoltInitialized;
	TempAllocatorImpl* m_pTempAllocator;
	JobSystemThreadPool* m_pJobSystem;

	PhysicsSystem m_PhysicsSystem;

	BroadPhaseLayerInterfaceImpl* m_pBPLayerInterface;
	ObjectVsBroadPhaseLayerFilterImpl* m_pObjectVsBPLayerFilter;
	ObjectLayerPairFilterImpl* m_pObjectLayerPairFilter;
	ContactListenerImpl* m_pContactListener;

	vector3 m_vGravity;
	LayerCollisionMaskArray m_arrLayerCollisionMasks;

	_float m_fFixedDeltaTime;   
	_float m_fAccumulator;    
	_float m_fMaxFrameDelta;    
	_uint m_iMaxSubSteps;
	_int m_iCollisionSteps;

#ifndef _CLIENT_BUILD
	enum class DebugRaycastShape
	{
		Line,
		Box,
		Sphere
	};

	struct DebugRaycastDisplay
	{
		vector3 start;
		vector3 end;
		vector3 halfExtent = vector3::zero();
		quaternion rotation = quaternion::identity();
		_float radius = 0.f;
		DebugRaycastShape shape = DebugRaycastShape::Line;
		_bool hit = false;
		_float remainTime = 0.f;
	};

	vector<DebugRaycastDisplay> m_vDebugRaycasts;
	class CMeshBuffer* m_pLineMesh;
	class CMaterial* m_pLineMaterial;
	void AddDebugRaycastDisplay(const vector3& _start, const vector3& _end, const _bool _hit);
	void AddDebugRaycastDisplay(const BoxRay& _boxRay, const _bool _hit);
	void AddDebugRaycastDisplay(const SphereRay& _sphereRay, const _bool _hit);
#endif
};

NS_END