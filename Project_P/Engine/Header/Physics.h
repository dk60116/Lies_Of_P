#pragma once

#include "epch.h"
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
	static constexpr ObjectLayer NUM_LAYERS = 3;
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

public:
	void  SetFixedDeltaTime(const _float _fixedDt);  
	_float GetFixedDeltaTime() const;

	void  SetMaxSubSteps(const _uint _maxSubSteps); 
	void  ResetStepper();

public:
	PhysicsSystem& GetPhysicsSystem();
	void RemoveContactPairs(const BodyID& _bodyID);

	const _bool IsInitialized() const;

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

	_float m_fFixedDeltaTime;   
	_float m_fAccumulator;    
	_float m_fMaxFrameDelta;    
	_uint m_iMaxSubSteps;
	_int m_iCollisionSteps;
};

NS_END
