#include "epch.h"
#include "BoxCollier.h"

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

void CBoxCollider::OnDestroy()
{
    __super::OnDestroy();
}

void CBoxCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    // 기존 캐시 해제
    ReleaseShape();

    // 0/음수 방지 + half extent
    const float sx = max(m_vSize.x, 0.001f);
    const float sy = max(m_vSize.y, 0.001f);
    const float sz = max(m_vSize.z, 0.001f);

    const JPH::Vec3 halfExtent(sx * 0.5f, sy * 0.5f, sz * 0.5f);

    // Box Shape 생성
    JPH::BoxShapeSettings boxSettings(halfExtent);
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError())
    {
        m_bShapeDirty = false;
        return;
    }

    // ShapeResult는 Result<Ref<Shape>> 이므로, Get()으로 Ref를 받고 raw ptr을 꺼낸다
    JPH::Ref<JPH::Shape> boxRef = boxResult.Get();
    const JPH::Shape* baseShape = boxRef.GetPtr();

    // 멤버(raw ptr)로 들고 있을 거면 AddRef로 수명 확보
    baseShape->AddRef();

    // center 오프셋 적용(Unity BoxCollider.center)
    const bool centerIsZero =
        (m_vCenter.x == 0.f && m_vCenter.y == 0.f && m_vCenter.z == 0.f);

    if (!centerIsZero)
    {
        const JPH::Vec3 center(m_vCenter.x, m_vCenter.y, m_vCenter.z);

        JPH::RotatedTranslatedShapeSettings rtSettings(center, JPH::Quat::sIdentity(), baseShape);
        JPH::ShapeSettings::ShapeResult rtResult = rtSettings.Create();

        if (!rtResult.HasError())
        {
            // 최종 shape가 바뀌었으니 baseShape 참조는 내려준다
            baseShape->Release();

            JPH::Ref<JPH::Shape> rtRef = rtResult.Get();
            const JPH::Shape* rtShape = rtRef.GetPtr();
            rtShape->AddRef();

            m_pShape = rtShape;
        }
        else
        {
            // 오프셋 shape 생성 실패 시 base 유지
            m_pShape = baseShape;
        }
    }
    else
    {
        m_pShape = baseShape;
    }

    m_bShapeDirty = false;
}
