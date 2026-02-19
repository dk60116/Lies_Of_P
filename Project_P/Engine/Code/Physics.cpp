#include "epch.h"
#include "Physics.h"
#include "RigidBody.h"

namespace Engine
{
	class CPhysics::BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface
	{
	public:
		BroadPhaseLayerInterfaceImpl()
		{
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
			mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::MOVING;
		}

		uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_BP_LAYERS;
		}

		BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
		{
			return mObjectToBroadPhase[inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
		{
			if (inLayer == BroadPhaseLayers::NON_MOVING) return "NON_MOVING";
			if (inLayer == BroadPhaseLayers::MOVING)     return "MOVING";
			return "UNKNOWN";
		}
#endif

	private:
		BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	class CPhysics::ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter
	{
	public:
		_bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
			case Layers::NON_MOVING:
				return inLayer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING:
				return true;
			case Layers::SENSOR:
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
			if (inObject1 == Layers::SENSOR && inObject2 == Layers::SENSOR)
				return false;

			if (inObject1 == Layers::NON_MOVING)
				return (inObject2 == Layers::MOVING) || (inObject2 == Layers::SENSOR);

			if (inObject1 == Layers::MOVING)
				return (inObject2 == Layers::NON_MOVING) || (inObject2 == Layers::MOVING) || (inObject2 == Layers::SENSOR);

			if (inObject1 == Layers::SENSOR)
				return (inObject2 == Layers::NON_MOVING) || (inObject2 == Layers::MOVING);

			return false;
		}
	};

	class CPhysics::ContactListenerImpl final : public ContactListener
	{
	public:
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

			auto itA = m_ActivePairs.find(MakePairKey(bodyId1, bodyId2));
			if (itA == m_ActivePairs.end())
				return;

			PairState state = itA->second;
			m_ActivePairs.erase(itA);

			if (!state.a || !state.b)
				return;

			if (state.isTrigger)
			{
				state.a->OnTriggerExit(state.bCollider);
				state.b->OnTriggerExit(state.aCollider);
			}
			else
			{
				state.a->OnCollisionExit(state.bCollider);
				state.b->OnCollisionExit(state.aCollider);
			}
		}

	private:
		struct PairState
		{
			CRigidBody* a = nullptr;
			CRigidBody* b = nullptr;
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

		static CRigidBody* GetRigidBody(const Body& body)
		{
			return reinterpret_cast<CRigidBody*>(body.GetUserData());
		}

		void Dispatch(const Body& body1, const Body& body2, const _bool isEnter)
		{
			CRigidBody* rb1 = GetRigidBody(body1);
			CRigidBody* rb2 = GetRigidBody(body2);
			if (!rb1 || !rb2)
				return;

			const _bool trigger = body1.IsSensor() || body2.IsSensor();
			CCollider* col1 = rb1->GetEventCollider(trigger);
			CCollider* col2 = rb2->GetEventCollider(trigger);

			PairState state;
			state.a = rb1;
			state.b = rb2;
			state.aCollider = col1;
			state.bCollider = col2;
			state.isTrigger = trigger;

			const PairKey key = MakePairKey(body1.GetID(), body2.GetID());
			auto [it, inserted] = m_ActivePairs.insert({ key, state });
			if (!inserted)
				it->second = state;

			if (trigger)
			{
				if (isEnter || inserted)
				{
					rb1->OnTriggerEnter(col2);
					rb2->OnTriggerEnter(col1);
				}
				else
				{
					rb1->OnTriggerStay(col2);
					rb2->OnTriggerStay(col1);
				}
			}
			else
			{
				if (isEnter || inserted)
				{
					rb1->OnCollisionEnter(col2);
					rb2->OnCollisionEnter(col1);
				}
				else
				{
					rb1->OnCollisionStay(col2);
					rb2->OnCollisionStay(col1);
				}
			}
		}

		unordered_map<PairKey, PairState> m_ActivePairs;
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
	, m_fFixedDeltaTime(1.0f / 60.0f)
	, m_fAccumulator(0.0f)
	, m_fMaxFrameDelta(0.25f)
	, m_iMaxSubSteps(8)
	, m_iCollisionSteps(1)
{
}

CPhysics::~CPhysics()
{
}

CPhysics& CPhysics::GetInstance()
{
	static CPhysics inst;

	return inst;
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
	m_PhysicsSystem.SetGravity(Vec3(0.f, -9.81f, 0.f));
	m_PhysicsSystem.OptimizeBroadPhase();

	m_bJoltInitialized = true;
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
}

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

	m_fFixedDeltaTime = _fixedDt;
	m_fAccumulator = 0.0f;
}

_float CPhysics::GetFixedDeltaTime() const
{
	return m_fFixedDeltaTime;
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

vector<CPhysics::RAYCASTHIT> CPhysics::Raycast(const Ray& _ray)
{
    vector<RAYCASTHIT> hits;

    vector<CRenderer*> renders = CSceneManager::GetInstance().Get_CrtScene()->Get_MeshObjects();

	for (auto ren : renders)
	{
		CMeshBuffer* buffer = ren->Get_MeshBuffer();

		if (!buffer)
			continue;

		vector<VertexTexNormalTangentBuffer> vb = buffer->Get_VertexBuffer();
		vector<_uint> ib = buffer->Get_IndexBuffer();

		if (vb.size() <= 0 || ib.size() <= 0)
			continue;

		CGameObject* obj = ren->Get_GameObject();
		_matrix worldMatrix = obj->Get_Transform()->Get_WorldMatrix();

		for (_uint i = 0; i < ib.size(); i += 3)
		{
			vector3 p0 = XMVector3TransformCoord(XMLoadFloat3(&vb[ib[i]].position), worldMatrix);
			vector3 p1 = XMVector3TransformCoord(XMLoadFloat3(&vb[ib[i + 1]].position), worldMatrix);
			vector3 p2 = XMVector3TransformCoord(XMLoadFloat3(&vb[ib[i + 2]].position), worldMatrix);

			_float t = 0.f;
			vector3 normal;

			if (IntersectRayTriangle(_ray.origin, _ray.dir, p0, p1, p2, t, normal))
			{
				if (t < 0 || t > _ray.maxDist)
					continue;

				RAYCASTHIT hit;
				hit.isHit = true;
				hit.distance = t;
				hit.hitNormal = normal;
				hit.hitPos = _ray.origin + _ray.dir * t;
				hit.object = obj;

				hits.push_back(hit);
			}
		}
	}

	sort(hits.begin(), hits.end(), [](const RAYCASTHIT& a, const RAYCASTHIT& b) {return a.distance < b.distance; });
     
    return hits;
}

_bool CPhysics::IntersectRayTriangle(const vector3& rayOrigin, const vector3& rayDir, const vector3& v0, const vector3& v1, const vector3& v2, _float& t, vector3& hitNormal)
{
	const _float EPSILON = 0.000001f;

	vector3 edge1 = v1 - v0;
	vector3 edge2 = v2 - v0;

	vector3 h = rayDir.cross(edge2);
	_float a = edge1.dot(h);
	if (fabs(a) < EPSILON)
		return false;

	_float f = 1.0f / a;
	vector3 s = rayOrigin - v0;
	_float u = f * s.dot(h);
	if (u < 0.0f || u > 1.0f)
		return false;

	vector3 q = s.cross(edge1);
	_float v = f * rayDir.dot(q);
	if (v < 0.0f || u + v > 1.0f)
		return false;

	t = f * edge2.dot(q);
	if (t > EPSILON) {
		hitNormal = edge1.cross(edge2).normalized();
		return true;
	}
	
	return false;
}