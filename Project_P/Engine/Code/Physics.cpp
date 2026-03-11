#include "epch.h"
#include "Physics.h"
#include "RigidBody.h"
#include "Collider.h"
#ifndef _CLIENT_BUILD
#include "Camera.h"
#include "Resources.h"
#include "MeshBuffer.h"
#include "Material.h"
#endif
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <mutex>
#include <vector>

namespace Engine
{
	namespace
	{
		constexpr uint64 colliderUserDataFlag = 1ull;

		inline CGameObject* ResolveHitObject(const Body& _body)
		{
			CGameObject* obj = nullptr;
			const uint64 userData = _body.GetUserData();
			if (userData == 0)
				return obj;

			if ((userData & colliderUserDataFlag) != 0)
			{
				const uint64 ptrValue = userData & ~colliderUserDataFlag;
				CCollider* collider = reinterpret_cast<CCollider*>(static_cast<uintptr_t>(ptrValue));
				obj = collider ? collider->Get_GameObject() : nullptr;
			}
			else
			{
				CRigidBody* rigidBody = reinterpret_cast<CRigidBody*>(static_cast<uintptr_t>(userData));
				obj = rigidBody ? rigidBody->Get_GameObject() : nullptr;
			}

			return obj;
		}

		inline _bool PassLayerMask(CGameObject* _obj, const CSceneManager::LayerMask _mask)
		{
			if (_mask == 0)
				return true;
			if (!_obj)
				return false;

			return CSceneManager::GetInstance().ContainLayerMask(_obj->GetLayer(), _mask);
		}


	}
	class CPhysics::BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface
	{
	public:
		uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_BP_LAYERS;
		}

		BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
		{
			const CollisionObjectType type = CPhysics::GetCollisionObjectType(inLayer);
			return type == CollisionObjectType::NonMoving ? BroadPhaseLayers::NON_MOVING : BroadPhaseLayers::MOVING;
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
		{
			if (inLayer == BroadPhaseLayers::NON_MOVING) return "NON_MOVING";
			if (inLayer == BroadPhaseLayers::MOVING)     return "MOVING";
			return "UNKNOWN";
		}
#endif
	};

	class CPhysics::ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter
	{
	public:
		_bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
		{
			switch (CPhysics::GetCollisionObjectType(inLayer1))
			{
			case CollisionObjectType::NonMoving:
				return inLayer2 == BroadPhaseLayers::MOVING;
			case CollisionObjectType::Moving:
				return true;
			case CollisionObjectType::Sensor:
				return true;
			default:
				return false;
			}
		}
	};

	class CPhysics::ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter
	{
	public:
		_bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
		{
			const _uint sceneLayer1 = CPhysics::GetSceneLayerIndex(inObject1);
			const _uint sceneLayer2 = CPhysics::GetSceneLayerIndex(inObject2);
			if (!CPhysics::GetInstance().GetLayerCollisionEnabled(sceneLayer1, sceneLayer2))
				return false;

			const CollisionObjectType type1 = CPhysics::GetCollisionObjectType(inObject1);
			const CollisionObjectType type2 = CPhysics::GetCollisionObjectType(inObject2);

			if (type1 == CollisionObjectType::Sensor && type2 == CollisionObjectType::Sensor)
				return false;

			if (type1 == CollisionObjectType::NonMoving)
				return (type2 == CollisionObjectType::Moving) || (type2 == CollisionObjectType::Sensor);

			if (type1 == CollisionObjectType::Moving)
				return (type2 == CollisionObjectType::NonMoving) || (type2 == CollisionObjectType::Moving) || (type2 == CollisionObjectType::Sensor);

			if (type1 == CollisionObjectType::Sensor)
				return (type2 == CollisionObjectType::NonMoving) || (type2 == CollisionObjectType::Moving);

			return false;
		}
	};

	class CPhysics::ContactListenerImpl final : public ContactListener
	{
	public:
		void RemovePairsForBody(const BodyID& _bodyID)
		{
			vector<PairState> removedStates;
			{
				lock_guard<mutex> guard(m_ActivePairsMutex);

				for (auto it = m_ActivePairs.begin(); it != m_ActivePairs.end();)
				{
					PairState state = it->second;
					const _bool removePair = state.bodyA == _bodyID || state.bodyB == _bodyID;

					if (!removePair)
					{
						++it;
						continue;
					}

					removedStates.push_back(state);
					it = m_ActivePairs.erase(it);
				}
			}

			for (const PairState& state : removedStates)
			{
				if (state.aCollider)
					state.aCollider->EndContact();
				if (state.bCollider)
					state.bCollider->EndContact();
			}
		}

		ValidateResult OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset, const CollideShapeResult& inCollisionResult) override
		{
			return ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
		{
			Dispatch(inBody1, inBody2, true);
		}

		void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
		{
			Dispatch(inBody1, inBody2, false);
		}

		void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
		{
			const BodyID bodyId1 = inSubShapePair.GetBody1ID();
			const BodyID bodyId2 = inSubShapePair.GetBody2ID();

			PairState state;
			{
				lock_guard<mutex> guard(m_ActivePairsMutex);
				auto itA = m_ActivePairs.find(MakePairKey(bodyId1, bodyId2));
				if (itA == m_ActivePairs.end())
					return;

				state = itA->second;
				m_ActivePairs.erase(itA);
			}

			if (state.aCollider)
				state.aCollider->EndContact();
			if (state.bCollider)
				state.bCollider->EndContact();

			if (state.isTrigger)
			{
				DispatchTriggerExit(state.aRigidBody, state.aStandaloneCollider, state.bCollider);
				DispatchTriggerExit(state.bRigidBody, state.bStandaloneCollider, state.aCollider);
			}
			else
			{
				DispatchCollisionExit(state.aRigidBody, state.aStandaloneCollider, state.bCollider);
				DispatchCollisionExit(state.bRigidBody, state.bStandaloneCollider, state.aCollider);
			}
		}

	private:
		static constexpr uint64 COLLIDER_USER_DATA_FLAG = 1ull;

		struct PairState
		{
			BodyID bodyA;
			BodyID bodyB;
			CRigidBody* aRigidBody = nullptr;
			CRigidBody* bRigidBody = nullptr;
			CCollider* aStandaloneCollider = nullptr;
			CCollider* bStandaloneCollider = nullptr;
			CCollider* aCollider = nullptr;
			CCollider* bCollider = nullptr;
			_bool isTrigger = false;
		};

		using PairKey = uint64;

		static PairKey MakePairKey(const BodyID& a, const BodyID& b)
		{
			const uint32 aIndex = a.GetIndexAndSequenceNumber();
			const uint32 bIndex = b.GetIndexAndSequenceNumber();
			const uint32 low = min(aIndex, bIndex);
			const uint32 high = max(aIndex, bIndex);
			return (static_cast<uint64>(low) << 32) | static_cast<uint64>(high);
		}

		static void DecodeUserData(const Body& _body, CRigidBody*& _outRigidBody, CCollider*& _outStandaloneCollider)
		{
			_outRigidBody = nullptr;
			_outStandaloneCollider = nullptr;

			const uint64 userData = _body.GetUserData();
			if (userData == 0)
				return;

			if ((userData & COLLIDER_USER_DATA_FLAG) != 0)
			{
				const uint64 ptrValue = userData & ~COLLIDER_USER_DATA_FLAG;
				_outStandaloneCollider = reinterpret_cast<CCollider*>(static_cast<uintptr_t>(ptrValue));
				return;
			}

			_outRigidBody = reinterpret_cast<CRigidBody*>(static_cast<uintptr_t>(userData));
		}

		static CCollider* ResolveEventCollider(CRigidBody* _rigidBody, CCollider* _standaloneCollider, const _bool _triggerEvent)
		{
			if (_rigidBody)
				return _rigidBody->GetEventCollider(_triggerEvent);

			return _standaloneCollider;
		}

		static void DispatchStandaloneCollisionEnter(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnCollisionEnter(_other);
			}
		}

		static void DispatchStandaloneCollisionStay(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnCollisionStay(_other);
			}
		}

		static void DispatchStandaloneCollisionExit(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnCollisionExit(_other);
			}
		}

		static void DispatchStandaloneTriggerEnter(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnTriggerEnter(_other);
			}
		}

		static void DispatchStandaloneTriggerStay(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnTriggerStay(_other);
			}
		}

		static void DispatchStandaloneTriggerExit(CCollider* _standaloneCollider, CCollider* _other)
		{
			if (!_standaloneCollider || !_standaloneCollider->Get_GameObject())
				return;

			auto& components = _standaloneCollider->Get_GameObject()->Get_ComponentList();
			for (TRAVERSAL_ITER(components, it))
			{
				CComponent* component = *it;
				if (!component || !component->Get_Enable())
					continue;

				component->OnTriggerExit(_other);
			}
		}

		static void DispatchCollisionEnter(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnCollisionEnter(_other);
				return;
			}

			DispatchStandaloneCollisionEnter(_standaloneCollider, _other);
		}

		static void DispatchCollisionStay(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnCollisionStay(_other);
				return;
			}

			DispatchStandaloneCollisionStay(_standaloneCollider, _other);
		}

		static void DispatchCollisionExit(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnCollisionExit(_other);
				return;
			}

			DispatchStandaloneCollisionExit(_standaloneCollider, _other);
		}

		static void DispatchTriggerEnter(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnTriggerEnter(_other);
				return;
			}

			DispatchStandaloneTriggerEnter(_standaloneCollider, _other);
		}

		static void DispatchTriggerStay(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnTriggerStay(_other);
				return;
			}

			DispatchStandaloneTriggerStay(_standaloneCollider, _other);
		}

		static void DispatchTriggerExit(CRigidBody* _rigidBody, CCollider* _standaloneCollider, CCollider* _other)
		{
			if (_rigidBody)
			{
				_rigidBody->OnTriggerExit(_other);
				return;
			}

			DispatchStandaloneTriggerExit(_standaloneCollider, _other);
		}

		void Dispatch(const Body& body1, const Body& body2, const _bool isEnter)
		{
			CRigidBody* rb1 = nullptr;
			CRigidBody* rb2 = nullptr;
			CCollider* standaloneCol1 = nullptr;
			CCollider* standaloneCol2 = nullptr;
			DecodeUserData(body1, rb1, standaloneCol1);
			DecodeUserData(body2, rb2, standaloneCol2);

			if (!rb1 && !rb2)
				return;

			const _bool trigger = body1.IsSensor() || body2.IsSensor();
			CCollider* col1 = ResolveEventCollider(rb1, standaloneCol1, trigger);
			CCollider* col2 = ResolveEventCollider(rb2, standaloneCol2, trigger);

			PairState state;
			state.bodyA = body1.GetID();
			state.bodyB = body2.GetID();
			state.aRigidBody = rb1;
			state.bRigidBody = rb2;
			state.aStandaloneCollider = standaloneCol1;
			state.bStandaloneCollider = standaloneCol2;
			state.aCollider = col1;
			state.bCollider = col2;
			state.isTrigger = trigger;

			const PairKey key = MakePairKey(body1.GetID(), body2.GetID());
			_bool inserted = false;
			{
				lock_guard<mutex> guard(m_ActivePairsMutex);
				auto [it, wasInserted] = m_ActivePairs.insert({ key, state });
				inserted = wasInserted;
				if (!inserted)
					it->second = state;
			}

			if (inserted)
			{
				if (col1)
					col1->BeginContact();
				if (col2)
					col2->BeginContact();
			}

			if (trigger)
			{
				if (isEnter || inserted)
				{
					DispatchTriggerEnter(rb1, standaloneCol1, col2);
					DispatchTriggerEnter(rb2, standaloneCol2, col1);
				}
				else
				{
					DispatchTriggerStay(rb1, standaloneCol1, col2);
					DispatchTriggerStay(rb2, standaloneCol2, col1);
				}
			}
			else
			{
				if (isEnter || inserted)
				{
					DispatchCollisionEnter(rb1, standaloneCol1, col2);
					DispatchCollisionEnter(rb2, standaloneCol2, col1);
				}
				else
				{
					DispatchCollisionStay(rb1, standaloneCol1, col2);
					DispatchCollisionStay(rb2, standaloneCol2, col1);
				}
			}
		}

		unordered_map<PairKey, PairState> m_ActivePairs;
		mutex m_ActivePairsMutex;
	};

	static void JoltTraceImpl(const char* fmt, ...)
	{
		char buf[2048];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);

		OutputDebugStringA(buf);
		OutputDebugStringA("\n");
	}

	static _bool JoltAssertFailedImpl(const char* expr, const char* msg, const char* file, JPH::uint line)
	{
		char buf[2048];
		snprintf(buf, sizeof(buf), "Jolt ASSERT: %s | %s (%s:%u)\n",
			expr, msg ? msg : "", file, line);
		OutputDebugStringA(buf);

		return true;
	}
}

CPhysics::CPhysics()
	: m_bJoltInitialized(false)
	, m_pTempAllocator(nullptr)
	, m_pJobSystem(nullptr)
	, m_pBPLayerInterface(nullptr)
	, m_pObjectVsBPLayerFilter(nullptr)
	, m_pObjectLayerPairFilter(nullptr)
	, m_pContactListener(nullptr)
	, m_vGravity(0.f, -9.81f, 0.f)
	, m_fFixedDeltaTime(1.0f / 60.0f)
	, m_fAccumulator(0.0f)
	, m_fMaxFrameDelta(0.25f)
	, m_iMaxSubSteps(8)
	, m_iCollisionSteps(1)
#ifndef _CLIENT_BUILD
	, m_pLineMesh(nullptr)
	, m_pLineMaterial(nullptr)
#endif
{
	for (_uint i = 0u; i < MAX_SCENE_LAYERS; ++i)
		m_arrLayerCollisionMasks[i] = ~LayerCollisionMask(0u);
}

CPhysics::~CPhysics()
{
}

CPhysics& CPhysics::GetInstance()
{
	static CPhysics inst;

	return inst;
}

_uint CPhysics::LayerMaskToIndex(const _uint sceneLayerMask)
{
	if (sceneLayerMask == 0u)
		return 0u;

	for (_uint i = 1u; i < MAX_SCENE_LAYERS; ++i)
	{
		if ((sceneLayerMask & (_uint(1u) << i)) != 0u)
			return i;
	}

	return 0u;
}

ObjectLayer CPhysics::MakeObjectLayer(const _uint sceneLayerMask, const CollisionObjectType type)
{
	const _uint sceneLayerIndex = min<_uint>(LayerMaskToIndex(sceneLayerMask), MAX_SCENE_LAYERS - 1u);
	const ObjectLayer typeIndex = static_cast<ObjectLayer>(type);
	return static_cast<ObjectLayer>(sceneLayerIndex * Layers::NUM_TYPES + typeIndex);
}

CPhysics::CollisionObjectType CPhysics::GetCollisionObjectType(const ObjectLayer layer)
{
	return static_cast<CollisionObjectType>(static_cast<_uint>(layer) % Layers::NUM_TYPES);
}

_uint CPhysics::GetSceneLayerIndex(const ObjectLayer layer)
{
	const _uint sceneLayerIndex = static_cast<_uint>(layer) / Layers::NUM_TYPES;
	return min<_uint>(sceneLayerIndex, MAX_SCENE_LAYERS - 1u);
}

HRESULT CPhysics::Initialize()
{
	if (m_bJoltInitialized)
		return S_OK;

	RegisterDefaultAllocator();

	Trace = JoltTraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = JoltAssertFailedImpl;);

	if (Factory::sInstance == nullptr)
		Factory::sInstance = new Factory();

	RegisterTypes();

	if (!m_pBPLayerInterface)      
		m_pBPLayerInterface = new BroadPhaseLayerInterfaceImpl();
	if (!m_pObjectVsBPLayerFilter) 
		m_pObjectVsBPLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
	if (!m_pObjectLayerPairFilter) 
		m_pObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

	if (!m_pTempAllocator)
	{
		const uint tempSize = 16 * 1024 * 1024;
		m_pTempAllocator = new TempAllocatorImpl(tempSize);
	}

	if (!m_pJobSystem)
	{
		uint hw = thread::hardware_concurrency();
		if (hw == 0) hw = 4;
		uint numThreads = (hw > 1) ? (hw - 1) : 1;

		m_pJobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, numThreads);
	}

	const uint cMaxBodies = 10240;
	const uint cNumBodyMutexes = 0;     
	const uint cMaxBodyPairs = 10240;
	const uint cMaxContactConstraints = 10240;

	m_PhysicsSystem.Init
	(
		cMaxBodies,
		cNumBodyMutexes,
		cMaxBodyPairs,
		cMaxContactConstraints,
		*m_pBPLayerInterface,
		*m_pObjectVsBPLayerFilter,
		*m_pObjectLayerPairFilter
	);

	m_pContactListener = new ContactListenerImpl();
	m_PhysicsSystem.SetContactListener(m_pContactListener);
	m_PhysicsSystem.SetGravity(Vec3(m_vGravity.x, m_vGravity.y, m_vGravity.z));
	m_PhysicsSystem.OptimizeBroadPhase();

	m_bJoltInitialized = true;

#ifndef _CLIENT_BUILD
	if (!m_pLineMesh)
	{
		m_pLineMesh = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Line (Mesh Buffer)");
		if (m_pLineMesh)
			m_pLineMesh->AddRef();
	}

	if (!m_pLineMaterial)
	{
		m_pLineMaterial = CResources::GetInstance().CloneOnGame<CMaterial>(L"DefaultLineMaterial (Material)");
		if (m_pLineMaterial)
			m_pLineMaterial->AddRef();
	}
#endif

	return S_OK;
}

void CPhysics::Release()
{
	if (!m_bJoltInitialized)
		return;

	if (m_pJobSystem)
	{
		delete m_pJobSystem;
		m_pJobSystem = nullptr;
	}

	if (m_pTempAllocator)
	{
		delete m_pTempAllocator;
		m_pTempAllocator = nullptr;
	}

	if (m_pObjectLayerPairFilter)
	{
		delete m_pObjectLayerPairFilter;
		m_pObjectLayerPairFilter = nullptr;
	}

	if (m_pContactListener)
	{
		m_PhysicsSystem.SetContactListener(nullptr);
		delete m_pContactListener;
		m_pContactListener = nullptr;
	}

	if (m_pObjectVsBPLayerFilter)
	{
		delete m_pObjectVsBPLayerFilter;
		m_pObjectVsBPLayerFilter = nullptr;
	}

	if (m_pBPLayerInterface)
	{
		delete m_pBPLayerInterface;
		m_pBPLayerInterface = nullptr;
	}

	UnregisterTypes();

	if (Factory::sInstance)
	{
		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}

	m_bJoltInitialized = false;

#ifndef _CLIENT_BUILD
	Safe_Release(m_pLineMesh);
	Safe_Release(m_pLineMaterial);
	m_vDebugRaycasts.clear();
#endif
}

void CPhysics::Tick(_float _deltaSeconds)
{
	if (!m_bJoltInitialized)
		return;

	if (_deltaSeconds <= 0.0f)
		return;

	if (_deltaSeconds > m_fMaxFrameDelta)
		_deltaSeconds = m_fMaxFrameDelta;

	m_fAccumulator += _deltaSeconds;

	_uint steps = 0;
	while (m_fAccumulator >= m_fFixedDeltaTime && steps < m_iMaxSubSteps)
	{
		Step(m_fFixedDeltaTime);
		m_fAccumulator -= m_fFixedDeltaTime;
		++steps;
	}

	if (steps == m_iMaxSubSteps)
		m_fAccumulator = 0.0f;

#ifndef _CLIENT_BUILD
	m_vDebugRaycasts.clear();
#endif
}

void CPhysics::RenderRaycastDebugDisplay()
{
#ifndef _CLIENT_BUILD
	if (m_vDebugRaycasts.empty() || !m_pLineMesh || !m_pLineMaterial)
		return;

	CCamera* camera = CSceneManager::GetInstance().Get_EditorCamera();

	if (!camera)
	{
		CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
		camera = scene ? scene->Get_Camera() : nullptr;
	}


	if (!camera)
		return;

	_float3 camPos = _float3();
	_matrix matView = camera->GetViewMatrix();
	_matrix matProj = camera->GetProjectionMatrix();

	auto buildLineWorld = [] (const vector3& a, const vector3& b, _matrix& outWorld)
	{
		const _vector va = XMVectorSet(a.x, a.y, a.z, 1.f);
		const _vector vb = XMVectorSet(b.x, b.y, b.z, 1.f);
		const _vector delta = XMVectorSubtract(vb, va);
		const _float length = XMVectorGetX(XMVector3Length(delta));
		if (length <= 0.0001f)
			return false;

		const _vector dir = XMVector3Normalize(delta);
		const _vector xAxis = XMVectorSet(1.f, 0.f, 0.f, 0.f);
		const _float dot = XMVectorGetX(XMVector3Dot(xAxis, dir));
		_matrix rot = XMMatrixIdentity();

		if (dot < 0.9999f)
		{
			if (dot > -0.9999f)
			{
				const _vector axis = XMVector3Normalize(XMVector3Cross(xAxis, dir));
				const _float angle = acosf(dot);
				rot = XMMatrixRotationAxis(axis, angle);
			}
			else
			{
				rot = XMMatrixRotationAxis(XMVectorSet(0.f, 1.f, 0.f, 0.f), XM_PI);
			}
		}

		const _vector mid = XMVectorScale(XMVectorAdd(va, vb), 0.5f);
		const _matrix scale = XMMatrixScaling(length, 1.f, 1.f);
		const _matrix trans = XMMatrixTranslationFromVector(mid);
		outWorld = scale * rot * trans;
		return true;
	};

	auto renderLine = [&] (const vector3& a, const vector3& b, const _float4& color)
	{
		_matrix world = XMMatrixIdentity();
		if (!buildLineWorld(a, b, world))
			return;

		m_pLineMaterial->Set_BaseColor(color);
		m_pLineMaterial->Bind_Matrix(world);
		m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
		m_pLineMesh->Render();
	};

	auto rotatePoint = [] (const vector3& p, const quaternion& q)
	{
		const _vector v = XMVectorSet(p.x, p.y, p.z, 0.0f);
		const _vector qv = XMVectorSet(q.x, q.y, q.z, q.w);
		const _vector r = XMVector3Rotate(v, qv);
		return vector3(XMVectorGetX(r), XMVectorGetY(r), XMVectorGetZ(r));
	};

	auto drawWireBox3D = [&] (const vector3& center, const vector3& halfExtent, const quaternion& rotation, const _float4& color)
	{
		vector3 corners[8] =
		{
			vector3(-halfExtent.x, -halfExtent.y, -halfExtent.z),
			vector3(halfExtent.x, -halfExtent.y, -halfExtent.z),
			vector3(halfExtent.x, halfExtent.y, -halfExtent.z),
			vector3(-halfExtent.x, halfExtent.y, -halfExtent.z),
			vector3(-halfExtent.x, -halfExtent.y, halfExtent.z),
			vector3(halfExtent.x, -halfExtent.y, halfExtent.z),
			vector3(halfExtent.x, halfExtent.y, halfExtent.z),
			vector3(-halfExtent.x, halfExtent.y, halfExtent.z)
		};

		for (_uint i = 0; i < 8; ++i)
			corners[i] = center + rotatePoint(corners[i], rotation);

		constexpr _int edge[12][2] =
		{
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		for (_int i = 0; i < 12; ++i)
			renderLine(corners[edge[i][0]], corners[edge[i][1]], color);
	};

	auto drawCircle3D = [&] (const vector3& center, const vector3& axisA, const vector3& axisB, const _float radius, const _float4& color)
	{
		const _uint segmentCount = 48u;
		for (_uint i = 0; i < segmentCount; ++i)
		{
			const _float t0 = XM_2PI * static_cast<_float>(i) / static_cast<_float>(segmentCount);
			const _float t1 = XM_2PI * static_cast<_float>(i + 1) / static_cast<_float>(segmentCount);
			const vector3 p0 = center + (axisA * cosf(t0) + axisB * sinf(t0)) * radius;
			const vector3 p1 = center + (axisA * cosf(t1) + axisB * sinf(t1)) * radius;
			renderLine(p0, p1, color);
		}
	};

	vector3 camRight = camera->Get_Transform()->Get_Directions().right;
	vector3 camUp = camera->Get_Transform()->Get_Directions().up;
	const _float camRightLen2 = camRight.x * camRight.x + camRight.y * camRight.y + camRight.z * camRight.z;
	if (camRightLen2 <= 0.000001f)
		camRight = vector3::right();
	else
		camRight = camRight.normalized();
	const _float camUpLen2 = camUp.x * camUp.x + camUp.y * camUp.y + camUp.z * camUp.z;
	if (camUpLen2 <= 0.000001f)
		camUp = vector3::up();
	else
		camUp = camUp.normalized();

	for (const DebugRaycastDisplay& debugRay : m_vDebugRaycasts)
	{
		const _float4 color = debugRay.hit ? _float4(0.314f, 1.f, 0.47f, 1.f) : _float4(1.f, 0.314f, 0.314f, 1.f);
		renderLine(debugRay.start, debugRay.end, color);

		if (debugRay.shape == DebugRaycastShape::Box)
		{
			drawWireBox3D(debugRay.start, debugRay.halfExtent, debugRay.rotation, color);
			drawWireBox3D(debugRay.end, debugRay.halfExtent, debugRay.rotation, color);
		}
		else if (debugRay.shape == DebugRaycastShape::Sphere)
		{
			drawCircle3D(debugRay.start, camRight, camUp, debugRay.radius, color);
			drawCircle3D(debugRay.end, camRight, camUp, debugRay.radius, color);
		}
	}
#endif
}

void CPhysics::ClearRaycastDebugDisplay()
{
#ifndef _CLIENT_BUILD
	m_vDebugRaycasts.clear();
#endif
}

#ifndef _CLIENT_BUILD
void CPhysics::AddDebugRaycastDisplay(const vector3& _start, const vector3& _end, const _bool _hit)
{
	for (DebugRaycastDisplay& debugRay : m_vDebugRaycasts)
	{
		if (debugRay.shape != DebugRaycastShape::Line)
			continue;
		if (debugRay.start.x != _start.x || debugRay.start.y != _start.y || debugRay.start.z != _start.z)
			continue;
		if (debugRay.end.x != _end.x || debugRay.end.y != _end.y || debugRay.end.z != _end.z)
			continue;
		debugRay.hit = debugRay.hit || _hit;
		return;
	}

	DebugRaycastDisplay debugRay;
	debugRay.start = _start;
	debugRay.end = _end;
	debugRay.shape = DebugRaycastShape::Line;
	debugRay.hit = _hit;
	debugRay.remainTime = 1.0f;
	m_vDebugRaycasts.push_back(debugRay);
}

void CPhysics::AddDebugRaycastDisplay(const BoxRay& _boxRay, const _bool _hit)
{
	const vector3 end = _boxRay.center + _boxRay.dir * _boxRay.maxDist;
	for (DebugRaycastDisplay& debugRay : m_vDebugRaycasts)
	{
		if (debugRay.shape != DebugRaycastShape::Box)
			continue;
		if (debugRay.start.x != _boxRay.center.x || debugRay.start.y != _boxRay.center.y || debugRay.start.z != _boxRay.center.z)
			continue;
		if (debugRay.end.x != end.x || debugRay.end.y != end.y || debugRay.end.z != end.z)
			continue;
		if (debugRay.halfExtent.x != _boxRay.halfExtent.x || debugRay.halfExtent.y != _boxRay.halfExtent.y || debugRay.halfExtent.z != _boxRay.halfExtent.z)
			continue;
		if (debugRay.rotation.x != _boxRay.rotation.x || debugRay.rotation.y != _boxRay.rotation.y || debugRay.rotation.z != _boxRay.rotation.z || debugRay.rotation.w != _boxRay.rotation.w)
			continue;
		debugRay.hit = debugRay.hit || _hit;
		return;
	}

	DebugRaycastDisplay debugRay;
	debugRay.start = _boxRay.center;
	debugRay.end = end;
	debugRay.halfExtent = _boxRay.halfExtent;
	debugRay.rotation = _boxRay.rotation;
	debugRay.shape = DebugRaycastShape::Box;
	debugRay.hit = _hit;
	debugRay.remainTime = 1.0f;
	m_vDebugRaycasts.push_back(debugRay);
}

void CPhysics::AddDebugRaycastDisplay(const SphereRay& _sphereRay, const _bool _hit)
{
	const vector3 end = _sphereRay.center + _sphereRay.dir * _sphereRay.maxDist;
	for (DebugRaycastDisplay& debugRay : m_vDebugRaycasts)
	{
		if (debugRay.shape != DebugRaycastShape::Sphere)
			continue;
		if (debugRay.start.x != _sphereRay.center.x || debugRay.start.y != _sphereRay.center.y || debugRay.start.z != _sphereRay.center.z)
			continue;
		if (debugRay.end.x != end.x || debugRay.end.y != end.y || debugRay.end.z != end.z)
			continue;
		if (debugRay.radius != _sphereRay.radius)
			continue;
		debugRay.hit = debugRay.hit || _hit;
		return;
	}

	DebugRaycastDisplay debugRay;
	debugRay.start = _sphereRay.center;
	debugRay.end = end;
	debugRay.radius = _sphereRay.radius;
	debugRay.shape = DebugRaycastShape::Sphere;
	debugRay.hit = _hit;
	debugRay.remainTime = 1.0f;
	m_vDebugRaycasts.push_back(debugRay);
}
#endif

void CPhysics::Step(const _float _fixedDeltaSeconds)
{
	if (!m_bJoltInitialized)
		return;

	if (_fixedDeltaSeconds <= 0.0f)
		return;

	const EPhysicsUpdateError err =
		m_PhysicsSystem.Update((_float)_fixedDeltaSeconds, m_iCollisionSteps, m_pTempAllocator, m_pJobSystem);

	if (err != EPhysicsUpdateError::None)
	{
		OutputDebugStringA("Jolt PhysicsSystem::Update error\n");
	}
}

void CPhysics::SetFixedDeltaTime(const _float _fixedDt)
{
	if (_fixedDt <= 0.0f)
		return;

	if (m_fFixedDeltaTime == _fixedDt)
		return;

	m_fFixedDeltaTime = _fixedDt;
	m_fAccumulator = 0.0f;
}

_float CPhysics::GetFixedDeltaTime() const
{
	return m_fFixedDeltaTime;
}

const vector3 CPhysics::GetGravity() const
{
	return m_vGravity;
}

void CPhysics::SetGravity(const vector3& gravity)
{
	m_vGravity = gravity;

	if (m_bJoltInitialized)
		m_PhysicsSystem.SetGravity(Vec3(m_vGravity.x, m_vGravity.y, m_vGravity.z));
}

const CPhysics::LayerCollisionMaskArray& CPhysics::GetLayerCollisionMasks() const
{
	return m_arrLayerCollisionMasks;
}

void CPhysics::SetLayerCollisionMasks(const LayerCollisionMaskArray& masks)
{
	m_arrLayerCollisionMasks = masks;

	for (_uint row = 0u; row < MAX_SCENE_LAYERS; ++row)
	{
		LayerCollisionMask sanitized = 0u;
		for (_uint column = 0u; column < MAX_SCENE_LAYERS; ++column)
		{
			const LayerCollisionMask bit = (LayerCollisionMask(1u) << column);
			if ((m_arrLayerCollisionMasks[row] & bit) != 0u)
				sanitized |= bit;
		}
		m_arrLayerCollisionMasks[row] = sanitized;
	}

	for (_uint row = 0u; row < MAX_SCENE_LAYERS; ++row)
	{
		for (_uint column = row + 1u; column < MAX_SCENE_LAYERS; ++column)
		{
			const LayerCollisionMask rowBit = (LayerCollisionMask(1u) << column);
			const LayerCollisionMask columnBit = (LayerCollisionMask(1u) << row);
			const _bool enabled = ((m_arrLayerCollisionMasks[row] & rowBit) != 0u) || ((m_arrLayerCollisionMasks[column] & columnBit) != 0u);
			SetLayerCollisionEnabled(row, column, enabled);
		}
	}
}

const _bool CPhysics::GetLayerCollisionEnabled(const _uint layerAIndex, const _uint layerBIndex) const
{
	if (layerAIndex >= MAX_SCENE_LAYERS || layerBIndex >= MAX_SCENE_LAYERS)
		return false;

	const LayerCollisionMask bit = (LayerCollisionMask(1u) << layerBIndex);
	return (m_arrLayerCollisionMasks[layerAIndex] & bit) != 0u;
}

void CPhysics::SetLayerCollisionEnabled(const _uint layerAIndex, const _uint layerBIndex, const _bool enabled)
{
	if (layerAIndex >= MAX_SCENE_LAYERS || layerBIndex >= MAX_SCENE_LAYERS)
		return;

	const LayerCollisionMask bitA = (LayerCollisionMask(1u) << layerBIndex);
	const LayerCollisionMask bitB = (LayerCollisionMask(1u) << layerAIndex);

	if (enabled)
	{
		m_arrLayerCollisionMasks[layerAIndex] |= bitA;
		m_arrLayerCollisionMasks[layerBIndex] |= bitB;
	}
	else
	{
		m_arrLayerCollisionMasks[layerAIndex] &= ~bitA;
		m_arrLayerCollisionMasks[layerBIndex] &= ~bitB;
	}
}

void CPhysics::SetMaxSubSteps(const _uint _maxSubSteps)
{
	m_iMaxSubSteps = (_maxSubSteps == 0) ? 1 : _maxSubSteps;
}

void CPhysics::ResetStepper()
{
	m_fAccumulator = 0.f;
}

PhysicsSystem& CPhysics::GetPhysicsSystem()
{
	return m_PhysicsSystem;
}

const _bool CPhysics::IsInitialized() const
{
	return m_bJoltInitialized;
}

void CPhysics::RemoveContactPairs(const BodyID& _bodyID)
{
	if (!m_pContactListener)
		return;

	m_pContactListener->RemovePairsForBody(_bodyID);
}

vector<CPhysics::RAYCASTHIT> CPhysics::Raycast(const Ray& _ray, const CSceneManager::LayerMask _mask)
{
	vector<RAYCASTHIT> hits;

	if (!m_bJoltInitialized)
		return hits;

	if (_ray.maxDist <= 0.0f)
		return hits;

	const Vec3 direction(_ray.dir.x, _ray.dir.y, _ray.dir.z);
	if (direction.LengthSq() <= 0.0f)
		return hits;

	const RRayCast ray(RVec3(_ray.origin.x, _ray.origin.y, _ray.origin.z), direction * _ray.maxDist);
	AllHitCollisionCollector<CastRayCollector> collector;
	const RayCastSettings rayCastSettings;
	m_PhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, rayCastSettings, collector);

	if (!collector.HadHit())
	{
#ifndef _CLIENT_BUILD
		AddDebugRaycastDisplay(_ray.origin, _ray.origin + _ray.dir * _ray.maxDist, false);
#endif
		return hits;
	}

	const BodyLockInterfaceLocking& lockInterface = m_PhysicsSystem.GetBodyLockInterface();

	for (const RayCastResult& result : collector.mHits)
	{
		BodyLockRead bodyLock(lockInterface, result.mBodyID);
		if (!bodyLock.Succeeded())
			continue;

		const Body& body = bodyLock.GetBody();
		const _float distance = static_cast<_float>(result.mFraction * _ray.maxDist);

		RAYCASTHIT hit;
		hit.isHit = true;
		hit.distance = distance;
		hit.hitPos = _ray.origin + _ray.dir * distance;

		const SubShapeID subShapeID = result.mSubShapeID2;
		const Vec3 worldNormal = body.GetWorldSpaceSurfaceNormal(
			subShapeID,
			RVec3(hit.hitPos.x, hit.hitPos.y, hit.hitPos.z)
		);
		hit.hitNormal = vector3(worldNormal.GetX(), worldNormal.GetY(), worldNormal.GetZ()).normalized();

		CGameObject* obj = ResolveHitObject(body);
		if (!PassLayerMask(obj, _mask))
			continue;

		hit.object = obj;
		hits.push_back(hit);
	}

	sort(hits.begin(), hits.end(),
		[](const RAYCASTHIT& a, const RAYCASTHIT& b) { return a.distance < b.distance; });

	#ifndef _CLIENT_BUILD
	AddDebugRaycastDisplay(_ray.origin, _ray.origin + _ray.dir * _ray.maxDist, !hits.empty());
#endif

	return hits;
}


vector<CPhysics::RAYCASTHIT> CPhysics::BoxRaycast(const BoxRay& _boxRay, const CSceneManager::LayerMask _mask)
{
	vector<RAYCASTHIT> hits;

	if (!m_bJoltInitialized)
		return hits;

	if (_boxRay.maxDist <= 0.0f)
		return hits;

	const Vec3 direction(_boxRay.dir.x, _boxRay.dir.y, _boxRay.dir.z);
	if (direction.LengthSq() <= 0.0f)
		return hits;

	const _float hx = max(fabsf(_boxRay.halfExtent.x), 0.001f);
	const _float hy = max(fabsf(_boxRay.halfExtent.y), 0.001f);
	const _float hz = max(fabsf(_boxRay.halfExtent.z), 0.001f);

	BoxShapeSettings boxSettings(Vec3(hx, hy, hz));
	ShapeSettings::ShapeResult boxResult = boxSettings.Create();
	if (boxResult.HasError())
		return hits;

	Ref<Shape> boxRef = boxResult.Get();
	const Shape* boxShape = boxRef.GetPtr();

	RShapeCast shapeCast(
		boxShape,
		Vec3::sReplicate(1.0f),
		RMat44::sRotationTranslation(
			Quat(_boxRay.rotation.x, _boxRay.rotation.y, _boxRay.rotation.z, _boxRay.rotation.w),
			RVec3(_boxRay.center.x, _boxRay.center.y, _boxRay.center.z)),
		direction * _boxRay.maxDist);

	AllHitCollisionCollector<CastShapeCollector> collector;
	ShapeCastSettings settings;
	m_PhysicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, RVec3::sZero(), collector);

	if (!collector.HadHit())
	{
#ifndef _CLIENT_BUILD
		AddDebugRaycastDisplay(_boxRay, false);
#endif
		return hits;
	}

	const BodyLockInterfaceLocking& lockInterface = m_PhysicsSystem.GetBodyLockInterface();

	for (const ShapeCastResult& result : collector.mHits)
	{
		BodyLockRead bodyLock(lockInterface, result.mBodyID2);
		if (!bodyLock.Succeeded())
			continue;

		const Body& body = bodyLock.GetBody();
		const _float distance = static_cast<_float>(result.mFraction * _boxRay.maxDist);

		RAYCASTHIT hit;
		hit.isHit = true;
		hit.distance = distance;
		hit.hitPos = _boxRay.center + _boxRay.dir * distance;

		vector3 normal(result.mPenetrationAxis.GetX(), result.mPenetrationAxis.GetY(), result.mPenetrationAxis.GetZ());
		if (normal.lengthSq() > 0.0f)
			normal = normal.normalized();
		hit.hitNormal = -normal;

		CGameObject* obj = ResolveHitObject(body);
		if (!PassLayerMask(obj, _mask))
			continue;

		hit.object = obj;
		hits.push_back(hit);
	}

	sort(hits.begin(), hits.end(), [](const RAYCASTHIT& a, const RAYCASTHIT& b) { return a.distance < b.distance; });
	#ifndef _CLIENT_BUILD
	AddDebugRaycastDisplay(_boxRay, !hits.empty());
#endif

	return hits;
}

vector<CPhysics::RAYCASTHIT> CPhysics::SphereRaycast(const SphereRay& _sphereRay, const CSceneManager::LayerMask _mask)
{
	vector<RAYCASTHIT> hits;

	if (!m_bJoltInitialized)
		return hits;

	if (_sphereRay.maxDist <= 0.0f)
		return hits;

	const Vec3 direction(_sphereRay.dir.x, _sphereRay.dir.y, _sphereRay.dir.z);
	if (direction.LengthSq() <= 0.0f)
		return hits;

	const _float radius = max(fabsf(_sphereRay.radius), 0.001f);

	SphereShapeSettings sphereSettings(radius);
	ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();
	if (sphereResult.HasError())
		return hits;

	Ref<Shape> sphereRef = sphereResult.Get();
	const Shape* sphereShape = sphereRef.GetPtr();

	RShapeCast shapeCast(
		sphereShape,
		Vec3::sReplicate(1.0f),
		RMat44::sTranslation(RVec3(_sphereRay.center.x, _sphereRay.center.y, _sphereRay.center.z)),
		direction * _sphereRay.maxDist);

	AllHitCollisionCollector<CastShapeCollector> collector;
	ShapeCastSettings settings;
	m_PhysicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, RVec3::sZero(), collector);

	if (!collector.HadHit())
	{
#ifndef _CLIENT_BUILD
		AddDebugRaycastDisplay(_sphereRay, false);
#endif
		return hits;
	}

	const BodyLockInterfaceLocking& lockInterface = m_PhysicsSystem.GetBodyLockInterface();

	for (const ShapeCastResult& result : collector.mHits)
	{
		BodyLockRead bodyLock(lockInterface, result.mBodyID2);
		if (!bodyLock.Succeeded())
			continue;

		const Body& body = bodyLock.GetBody();
		const _float distance = static_cast<_float>(result.mFraction * _sphereRay.maxDist);

		RAYCASTHIT hit;
		hit.isHit = true;
		hit.distance = distance;
		hit.hitPos = _sphereRay.center + _sphereRay.dir * distance;

		vector3 normal(result.mPenetrationAxis.GetX(), result.mPenetrationAxis.GetY(), result.mPenetrationAxis.GetZ());
		if (normal.lengthSq() > 0.0f)
			normal = normal.normalized();
		hit.hitNormal = -normal;

		CGameObject* obj = ResolveHitObject(body);
		if (!PassLayerMask(obj, _mask))
			continue;

		hit.object = obj;
		hits.push_back(hit);
	}

	sort(hits.begin(), hits.end(), [](const RAYCASTHIT& a, const RAYCASTHIT& b) { return a.distance < b.distance; });
	#ifndef _CLIENT_BUILD
	AddDebugRaycastDisplay(_sphereRay, !hits.empty());
#endif

	return hits;
}
