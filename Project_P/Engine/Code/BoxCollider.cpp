#include "epch.h"
#include "BoxCollider.h"

CBoxCollider::CBoxCollider()
    : m_vSize(vector3::one())
{
}

CBoxCollider::~CBoxCollider()
{
}

CBoxCollider* CBoxCollider::Create()
{
    return new CBoxCollider();
}

CComponent* CBoxCollider::Clone() const
{
    CBoxCollider* clone = new CBoxCollider();

    clone->m_bIsTrigger = m_bIsTrigger;
    clone->m_vCenter = m_vCenter;
    clone->m_vSize = m_vSize;
    clone->m_bShapeDirty = true;

    return clone;
}

HRESULT CBoxCollider::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    return S_OK;
}

void CBoxCollider::Update()
{
}

void CBoxCollider::FixedUpdate()
{
}

void CBoxCollider::Render_Editor()
{
}

const vector3& CBoxCollider::GetSize() const
{
    return m_vSize;
}

void CBoxCollider::OnDestroy()
{
    __super::OnDestroy();
}

void CBoxCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    ReleaseShape();

    const float sx = max(m_vSize.x, 0.001f);
    const float sy = max(m_vSize.y, 0.001f);
    const float sz = max(m_vSize.z, 0.001f);

    const JPH::Vec3 halfExtent(sx * 0.5f, sy * 0.5f, sz * 0.5f);

    // Box Shape »ý¼º
    JPH::BoxShapeSettings boxSettings(halfExtent);
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError())
    {
        m_bShapeDirty = false;
        return;
    }

    JPH::Ref<JPH::Shape> boxRef = boxResult.Get();
    const JPH::Shape* baseShape = boxRef.GetPtr();

    baseShape->AddRef();

    const _bool centerIsZero =
        (m_vCenter.x == 0.f && m_vCenter.y == 0.f && m_vCenter.z == 0.f);

    if (!centerIsZero)
    {
        const JPH::Vec3 center(m_vCenter.x, m_vCenter.y, m_vCenter.z);

        JPH::RotatedTranslatedShapeSettings rtSettings(center, JPH::Quat::sIdentity(), baseShape);
        JPH::ShapeSettings::ShapeResult rtResult = rtSettings.Create();

        if (!rtResult.HasError())
        {
            baseShape->Release();

            JPH::Ref<JPH::Shape> rtRef = rtResult.Get();
            const JPH::Shape* rtShape = rtRef.GetPtr();
            rtShape->AddRef();

            m_pShape = rtShape;
        }
        else
        {
            m_pShape = baseShape;
        }
    }
    else
    {
        m_pShape = baseShape;
    }

    m_bShapeDirty = false;
}
