#pragma once
#include "epch.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

NS_BEGIN(Engine)

class CColliderManager final
{
    SINGLETONCLASS(CColliderManager);

public:
    HRESULT Initialize();
    void FixedUpdate();
    void Release();

public:
    JPH::PhysicsSystem& GetSystem() { return m_PhysicsSystem; }

private:
    class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        _uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_BP_LAYERS; }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            switch (inLayer)
            {
            case Layers::NON_MOVING: 
                return BroadPhaseLayers::NON_MOVING;
            case Layers::MOVING:     
                return BroadPhaseLayers::MOVING;
            case Layers::SENSOR:     
                return BroadPhaseLayers::MOVING;
            default:                 
                return BroadPhaseLayers::MOVING;
            }
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((uint8)inLayer)
            {
            case 0: return "NON_MOVING";
            case 1: return "MOVING";
            default: return "UNKNOWN";
            }
        }
#endif
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            if (inLayer1 == Layers::NON_MOVING)
                return inLayer2 == BroadPhaseLayers::MOVING;
            return true;
        }
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
        {
            if (inLayer1 == Layers::SENSOR || inLayer2 == Layers::SENSOR)
                return true;

            if (inLayer1 == Layers::NON_MOVING && inLayer2 == Layers::NON_MOVING)
                return false;

            return true;
        }
    };

    class BodyActivationListenerImpl final : public JPH::BodyActivationListener
    {
    public:
        void OnBodyActivated(const JPH::BodyID&, uint64_t) override {}
        void OnBodyDeactivated(const JPH::BodyID&, uint64_t) override {}
    };

    class ContactListenerImpl final : public JPH::ContactListener
    {
    public:
        JPH::ValidateResult OnContactValidate(
            const JPH::Body&, const JPH::Body&, JPH::RVec3Arg, const JPH::CollideShapeResult&) override
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(const JPH::Body&, const JPH::Body&, const JPH::ContactManifold&,
            JPH::ContactSettings&) override
        {
        }

        void OnContactPersisted(const JPH::Body&, const JPH::Body&, const JPH::ContactManifold&,
            JPH::ContactSettings&) override
        {
        }

        void OnContactRemoved(const JPH::SubShapeIDPair&) override
        {
        }
    };

private:
    BroadPhaseLayerInterfaceImpl m_BPLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl m_ObjectVsBPLayerFilter;
    ObjectLayerPairFilterImpl m_ObjectLayerPairFilter;

    JPH::TempAllocatorImpl* m_pTempAllocator;
    JPH::JobSystemThreadPool* m_pJobSystem;

    JPH::PhysicsSystem m_PhysicsSystem;

    BodyActivationListenerImpl m_BodyActivationListener;
    ContactListenerImpl m_ContactListener;

    _bool m_bInitialized;
};

NS_END

