#include "epch.h"
#include "CapsuleCollider.h"
#include "Resources.h"
#include "MeshBuffer.h"
#include "Material.h"
#include "SceneManager.h"
#include "Camera.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

namespace
{
    _bool BuildLineWorldMatrix(const _vector& a, const _vector& b, _matrix& outWorld)
    {
        _vector delta = b - a;
        _float length = XMVectorGetX(XMVector3Length(delta));

        if (length <= 0.0001f)
            return false;

        _vector dir = XMVector3Normalize(delta);
        _vector xAxis = XMVectorSet(1.f, 0.f, 0.f, 0.f);
        _float dot = XMVectorGetX(XMVector3Dot(xAxis, dir));
        _matrix rot = XMMatrixIdentity();

        if (dot < 0.9999f)
        {
            if (dot > -0.9999f)
            {
                _vector axis = XMVector3Normalize(XMVector3Cross(xAxis, dir));
                _float angle = acosf(dot);
                rot = XMMatrixRotationAxis(axis, angle);
            }
            else
            {
                rot = XMMatrixRotationAxis(XMVectorSet(0.f, 1.f, 0.f, 0.f), XM_PI);
            }
        }

        _vector mid = (a + b) * 0.5f;
        _matrix scale = XMMatrixScaling(length, 1.f, 1.f);
        _matrix trans = XMMatrixTranslationFromVector(mid);
        outWorld = scale * rot * trans;
        return true;
    }
}

CCapsuleCollider::CCapsuleCollider()
    : m_fRadius(0.5f)
    , m_fHeight(1.f)
    , m_pLineMesh(nullptr)
    , m_pLineMaterial(nullptr)
{
    m_strName = L"Capsule Collider";
}

CCapsuleCollider::~CCapsuleCollider()
{
}

CCapsuleCollider* CCapsuleCollider::Create()
{
    return new CCapsuleCollider();
}

CComponent* CCapsuleCollider::Clone() const
{
    CCapsuleCollider* clone = new CCapsuleCollider();

    clone->m_bIsTrigger = m_bIsTrigger;
    clone->m_vCenter = m_vCenter;
    clone->m_fRadius = m_fRadius;
    clone->m_fHeight = m_fHeight;
    clone->m_bShapeDirty = true;

    return clone;
}

HRESULT CCapsuleCollider::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

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
        {
            m_pLineMaterial->Set_BaseColor(_float4(0.f, 1.f, 0.f, 1.f));
            m_pLineMaterial->AddRef();
        }
    }
#endif

    return S_OK;
}

void CCapsuleCollider::Update()
{
}

void CCapsuleCollider::FixedUpdate()
{
}

void CCapsuleCollider::Render_Editor()
{
}

void CCapsuleCollider::Render_Gizmo()
{
#ifndef _CLIENT_BUILD
    if (!m_pLineMesh || !m_pLineMaterial)
        return;

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    if (!cam)
        return;

    const _float radius = max(m_fRadius, 0.001f);
    const _float halfHeight = max(m_fHeight * 0.5f, 0.f);
    const _uint segmentCount = 36u;

    const _matrix objectWorld = Get_Transform()->Get_WorldMatrix();
    const _matrix centerOffset = XMMatrixTranslation(m_vCenter.x, m_vCenter.y, m_vCenter.z);
    const _matrix world = centerOffset * objectWorld;

    _float3 camPos = _float3();
    _matrix matView = cam->Get_ViewMatrix();
    _matrix matProj = cam->Get_ProjectionMatrix();

    auto drawSegment = [&](const _vector& p0, const _vector& p1)
    {
        _matrix lineWorld = XMMatrixIdentity();
        if (!BuildLineWorldMatrix(p0, p1, lineWorld))
            return;

        m_pLineMaterial->Bind_Matrix(lineWorld);
        m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
        m_pLineMesh->Render();
    };

    for (_uint i = 0; i < segmentCount; ++i)
    {
        const _float t0 = (XM_2PI * static_cast<_float>(i)) / static_cast<_float>(segmentCount);
        const _float t1 = (XM_2PI * static_cast<_float>(i + 1)) / static_cast<_float>(segmentCount);

        _vector top0 = XMVectorSet(cosf(t0) * radius, halfHeight, sinf(t0) * radius, 1.f);
        _vector top1 = XMVectorSet(cosf(t1) * radius, halfHeight, sinf(t1) * radius, 1.f);
        _vector bottom0 = XMVectorSet(cosf(t0) * radius, -halfHeight, sinf(t0) * radius, 1.f);
        _vector bottom1 = XMVectorSet(cosf(t1) * radius, -halfHeight, sinf(t1) * radius, 1.f);

        top0 = XMVector3Transform(top0, world);
        top1 = XMVector3Transform(top1, world);
        bottom0 = XMVector3Transform(bottom0, world);
        bottom1 = XMVector3Transform(bottom1, world);

        drawSegment(top0, top1);
        drawSegment(bottom0, bottom1);
    }

    _vector axisTop[4] =
    {
        XMVectorSet(radius, halfHeight, 0.f, 1.f),
        XMVectorSet(-radius, halfHeight, 0.f, 1.f),
        XMVectorSet(0.f, halfHeight, radius, 1.f),
        XMVectorSet(0.f, halfHeight, -radius, 1.f)
    };
    _vector axisBottom[4] =
    {
        XMVectorSet(radius, -halfHeight, 0.f, 1.f),
        XMVectorSet(-radius, -halfHeight, 0.f, 1.f),
        XMVectorSet(0.f, -halfHeight, radius, 1.f),
        XMVectorSet(0.f, -halfHeight, -radius, 1.f)
    };

    for (_uint i = 0; i < 4; ++i)
    {
        _vector p0 = XMVector3Transform(axisTop[i], world);
        _vector p1 = XMVector3Transform(axisBottom[i], world);
        drawSegment(p0, p1);
    }

    const _uint hemiSegments = 18u;
    for (_uint i = 0; i < hemiSegments; ++i)
    {
        const _float t0 = (XM_PIDIV2 * static_cast<_float>(i)) / static_cast<_float>(hemiSegments);
        const _float t1 = (XM_PIDIV2 * static_cast<_float>(i + 1)) / static_cast<_float>(hemiSegments);

        _vector pxTop0 = XMVectorSet(cosf(t0) * radius, halfHeight + sinf(t0) * radius, 0.f, 1.f);
        _vector pxTop1 = XMVectorSet(cosf(t1) * radius, halfHeight + sinf(t1) * radius, 0.f, 1.f);
        _vector pzTop0 = XMVectorSet(0.f, halfHeight + sinf(t0) * radius, cosf(t0) * radius, 1.f);
        _vector pzTop1 = XMVectorSet(0.f, halfHeight + sinf(t1) * radius, cosf(t1) * radius, 1.f);

        _vector pxBottom0 = XMVectorSet(cosf(t0) * radius, -halfHeight - sinf(t0) * radius, 0.f, 1.f);
        _vector pxBottom1 = XMVectorSet(cosf(t1) * radius, -halfHeight - sinf(t1) * radius, 0.f, 1.f);
        _vector pzBottom0 = XMVectorSet(0.f, -halfHeight - sinf(t0) * radius, cosf(t0) * radius, 1.f);
        _vector pzBottom1 = XMVectorSet(0.f, -halfHeight - sinf(t1) * radius, cosf(t1) * radius, 1.f);

        drawSegment(XMVector3Transform(pxTop0, world), XMVector3Transform(pxTop1, world));
        drawSegment(XMVector3Transform(pzTop0, world), XMVector3Transform(pzTop1, world));
        drawSegment(XMVector3Transform(pxBottom0, world), XMVector3Transform(pxBottom1, world));
        drawSegment(XMVector3Transform(pzBottom0, world), XMVector3Transform(pzBottom1, world));
    }
#endif
}

const _float CCapsuleCollider::GetRadius() const
{
    return m_fRadius;
}

const _float CCapsuleCollider::GetHeight() const
{
    return m_fHeight;
}

void CCapsuleCollider::SetRadius(const _float radius)
{
    m_fRadius = max(radius, 0.001f);
    m_bShapeDirty = true;
}

void CCapsuleCollider::SetHeight(const _float height)
{
    m_fHeight = max(height, 0.f);
    m_bShapeDirty = true;
}

void CCapsuleCollider::OnDestroy()
{
    Safe_Release(m_pLineMesh);
    Safe_Release(m_pLineMaterial);

    __super::OnDestroy();
}

void CCapsuleCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    ReleaseShape();

    const _float radius = max(m_fRadius, 0.001f);
    const _float halfHeight = max(m_fHeight * 0.5f, 0.f);

    CapsuleShapeSettings capsuleSettings(halfHeight, radius);
    ShapeSettings::ShapeResult capsuleResult = capsuleSettings.Create();

    if (capsuleResult.HasError())
    {
        m_bShapeDirty = false;
        return;
    }

    Ref<Shape> capsuleRef = capsuleResult.Get();
    const Shape* baseShape = capsuleRef.GetPtr();
    baseShape->AddRef();

    const _bool centerIsZero =
        (m_vCenter.x == 0.f && m_vCenter.y == 0.f && m_vCenter.z == 0.f);

    if (!centerIsZero)
    {
        const Vec3 center(m_vCenter.x, m_vCenter.y, m_vCenter.z);

        RotatedTranslatedShapeSettings rtSettings(center, JPH::Quat::sIdentity(), baseShape);
        ShapeSettings::ShapeResult rtResult = rtSettings.Create();

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
