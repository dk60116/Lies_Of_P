#include "epch.h"
#include "Physics.h"

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
{
}

CPhysics::~CPhysics()
{
	Release();
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
		uint hw = std::thread::hardware_concurrency();
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
	const float EPSILON = 0.000001f;

	vector3 edge1 = v1 - v0;
	vector3 edge2 = v2 - v0;

	vector3 h = rayDir.cross(edge2);
	float a = edge1.dot(h);
	if (fabs(a) < EPSILON)
		return false;

	float f = 1.0f / a;
	vector3 s = rayOrigin - v0;
	float u = f * s.dot(h);
	if (u < 0.0f || u > 1.0f)
		return false;

	vector3 q = s.cross(edge1);
	float v = f * rayDir.dot(q);
	if (v < 0.0f || u + v > 1.0f)
		return false;

	t = f * edge2.dot(q);
	if (t > EPSILON) {
		hitNormal = edge1.cross(edge2).normalized();
		return true;
	}
	
	return false;
}