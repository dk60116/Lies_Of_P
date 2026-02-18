#include "epch.h"

CColliderManager::CColliderManager()
	: m_BPLayerInterface()
	, m_ObjectVsBPLayerFilter()
	, m_ObjectLayerPairFilter()
	, m_pTempAllocator(nullptr)
	, m_pJobSystem(nullptr)
	, m_PhysicsSystem()
	, m_BodyActivationListener()
	, m_ContactListener()
	, m_bInitialized(false)
    , m_EventMutex()
    , m_Events({})
    , m_ActivePairs({})
    , m_RigidBodies({})
{
}

CColliderManager::~CColliderManager()
{
	Release();
}

CColliderManager& CColliderManager::GetInstance()
{
	static CColliderManager inst;
	return inst;
}

HRESULT CColliderManager::Initialize()
{
    if (m_bInitialized)
        return S_OK;

    RegisterDefaultAllocator();

    if (Factory::sInstance == nullptr)
        Factory::sInstance = new Factory();

    RegisterTypes();

    constexpr _uint kTempAllocatorSize = 16 * 1024 * 1024;
    m_pTempAllocator = new JPH::TempAllocatorImpl(kTempAllocatorSize);

    const _uint hw = max(1u, thread::hardware_concurrency());
    const _uint num_threads = max(1u, hw - 1);
    m_pJobSystem = new JPH::JobSystemThreadPool
    (
        cMaxPhysicsJobs,
        cMaxPhysicsBarriers,
        num_threads
    );

    const _uint max_bodies = 8192;
    const _uint num_body_mutexes = 0;
    const _uint max_body_pairs = 65536;
    const _uint max_contact_constraints = 16384;

    m_PhysicsSystem.Init
    (
        max_bodies,
        num_body_mutexes,
        max_body_pairs,
        max_contact_constraints,
        m_BPLayerInterface,
        m_ObjectVsBPLayerFilter,
        m_ObjectLayerPairFilter
    );

    m_PhysicsSystem.SetBodyActivationListener(&m_BodyActivationListener);
    m_PhysicsSystem.SetContactListener(&m_ContactListener);

    m_bInitialized = true;
    return S_OK;
}

void CColliderManager::FixedUpdate()
{
    if (!m_bInitialized)
        return;

    constexpr _int collisionSteps = 1;

    m_PhysicsSystem.Update(DELTA_TIME, collisionSteps, m_pTempAllocator, m_pJobSystem);
}

void CColliderManager::Release()
{
    if (!m_bInitialized)
        return;

    delete m_pJobSystem;     
    m_pJobSystem = nullptr;
    delete m_pTempAllocator;
    m_pTempAllocator = nullptr;

    UnregisterTypes();

    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    m_bInitialized = false;
}

PhysicsSystem& CColliderManager::GetSystem()
{
    return m_PhysicsSystem;
}

void CColliderManager::RegisterRigidBody(CRigidBody* _rb)
{
    if (_rb)
        m_RigidBodies.push_back(_rb);
}

void CColliderManager::UnregisterRigidBody(CRigidBody* _rb)
{
    if (_rb)
        m_RigidBodies.remove(_rb);
}
