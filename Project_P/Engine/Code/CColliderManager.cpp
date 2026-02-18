#include "epch.h"
#include <Jolt/Physics/Body/BodyLock.h>

CColliderManager::CColliderManager()
	: m_BPLayerInterface()
	, m_ObjectVsBPLayerFilter()
	, m_ObjectLayerPairFilter()
	, m_pTempAllocator(nullptr)
	, m_pJobSystem(nullptr)
	, m_PhysicsSystem()
	, m_BodyActivationListener()
	 , m_ContactListener(this)
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

    m_ContactListener.SetOwner(this);
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
    DispatchQueuedEvents();
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

    {
        lock_guard<mutex> lock(m_EventMutex);
        m_Events.clear();
        m_ActivePairs.clear();
    }

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

void CColliderManager::QueueContactEvent(const BodyID& a, const BodyID& b, EContactType type, bool isTriggerHint)
{
    if (!m_bInitialized)
        return;

    BodyLockRead lockA(m_PhysicsSystem.GetBodyLockInterfaceNoLock(), a);
    BodyLockRead lockB(m_PhysicsSystem.GetBodyLockInterfaceNoLock(), b);

    if (!lockA.Succeeded() || !lockB.Succeeded())
        return;

    const Body& bodyA = lockA.GetBody();
    const Body& bodyB = lockB.GetBody();

    CRigidBody* rbA = reinterpret_cast<CRigidBody*>(bodyA.GetUserData());
    CRigidBody* rbB = reinterpret_cast<CRigidBody*>(bodyB.GetUserData());

    if (!rbA || !rbB || rbA == rbB)
        return;

    const bool isTrigger = isTriggerHint || bodyA.IsSensor() || bodyB.IsSensor();

    lock_guard<mutex> lock(m_EventMutex);

    const uint64_t key = MakePairKey(a, b);

    if (type == EContactType::Enter)
    {
        m_ActivePairs[key] = { rbA, rbB, isTrigger };
    }
    else if (type == EContactType::Stay)
    {
        auto it = m_ActivePairs.find(key);
        if (it == m_ActivePairs.end())
            m_ActivePairs[key] = { rbA, rbB, isTrigger };
        else
            it->second.isTrigger = it->second.isTrigger || isTrigger;
    }

    m_Events.push_back({ rbA, rbB, isTrigger, key, type });
}

void CColliderManager::DispatchQueuedEvents()
{
    vector<ContactEvent> events;

    {
        lock_guard<mutex> lock(m_EventMutex);
        if (m_Events.empty())
            return;

        events.swap(m_Events);
    }

    for (const ContactEvent& ev : events)
    {
        if (!ev.a || !ev.b)
            continue;

        if (ev.type == EContactType::Exit)
        {
            const uint64_t key = ev.key;
            PairInfo info = { ev.a, ev.b, ev.isTrigger };

            {
                lock_guard<mutex> lock(m_EventMutex);
                auto it = m_ActivePairs.find(key);
                if (it != m_ActivePairs.end())
                {
                    info = it->second;
                    m_ActivePairs.erase(it);
                }
            }

            if (info.isTrigger)
            {
                info.a->OnTriggerExit(nullptr);
                info.b->OnTriggerExit(nullptr);
            }
            else
            {
                info.a->OnCollisionExit(nullptr);
                info.b->OnCollisionExit(nullptr);
            }

            continue;
        }

        if (ev.isTrigger)
        {
            if (ev.type == EContactType::Enter)
            {
                ev.a->OnTriggerEnter(nullptr);
                ev.b->OnTriggerEnter(nullptr);
            }
            else
            {
                ev.a->OnTriggerStay(nullptr);
                ev.b->OnTriggerStay(nullptr);
            }
        }
        else
        {
            if (ev.type == EContactType::Enter)
            {
                ev.a->OnCollisionEnter(nullptr);
                ev.b->OnCollisionEnter(nullptr);
            }
            else
            {
                ev.a->OnCollisionStay(nullptr);
                ev.b->OnCollisionStay(nullptr);
            }
        }
    }
}
