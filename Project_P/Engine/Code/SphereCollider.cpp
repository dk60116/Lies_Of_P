#include "epch.h"
#include "SphereCollider.h"
#include "Resources.h"
#include "MeshBuffer.h"
#include "Material.h"
#include "SceneManager.h"
#include "Camera.h"
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

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

CSphereCollider::CSphereCollider()
    : m_fRadius(0.5f)
    , m_pLineMesh(nullptr)
    , m_pLineMaterial(nullptr)
{
    m_strName = L"Sphere Collider";
}

CSphereCollider::~CSphereCollider()
{
}

CSphereCollider* CSphereCollider::Create()
{
    return new CSphereCollider();
}

CComponent* CSphereCollider::Clone() const
{
    CSphereCollider* clone = new CSphereCollider();

    clone->m_bIsTrigger = m_bIsTrigger;
    clone->m_vCenter = m_vCenter;
    clone->m_fRadius = m_fRadius;
    clone->m_bShapeDirty = true;

    return clone;
}

HRESULT CSphereCollider::Initialize()
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

void CSphereCollider::Update()
{
}

void CSphereCollider::FixedUpdate()
{
}

void CSphereCollider::Render_Editor()
{
}

void CSphereCollider::Render_Gizmo()
{
#ifndef _CLIENT_BUILD
    if (!m_pLineMesh || !m_pLineMaterial)
        return;

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    if (!cam)
        return;

    const _float radius = max(m_fRadius, 0.001f);
    const _uint segmentCount = 36u;
    const _matrix objectWorld = Get_Transform()->Get_WorldMatrix();
    const _matrix centerOffset = XMMatrixTranslation(m_vCenter.x, m_vCenter.y, m_vCenter.z);
    const _matrix world = centerOffset * objectWorld;

    _float3 camPos = _float3();
    _matrix matView = cam->Get_ViewMatrix();
    _matrix matProj = cam->Get_ProjectionMatrix();

    auto drawCircle = [&](_int axis)
    {
        for (_uint i = 0; i < segmentCount; ++i)
        {
            const _float t0 = (XM_2PI * static_cast<_float>(i)) / static_cast<_float>(segmentCount);
            const _float t1 = (XM_2PI * static_cast<_float>(i + 1)) / static_cast<_float>(segmentCount);

            _vector p0 = XMVectorZero();
            _vector p1 = XMVectorZero();

            if (axis == 0)
            {
                p0 = XMVectorSet(0.f, cosf(t0) * radius, sinf(t0) * radius, 1.f);
                p1 = XMVectorSet(0.f, cosf(t1) * radius, sinf(t1) * radius, 1.f);
            }
            else if (axis == 1)
            {
                p0 = XMVectorSet(cosf(t0) * radius, 0.f, sinf(t0) * radius, 1.f);
                p1 = XMVectorSet(cosf(t1) * radius, 0.f, sinf(t1) * radius, 1.f);
            }
            else
            {
                p0 = XMVectorSet(cosf(t0) * radius, sinf(t0) * radius, 0.f, 1.f);
                p1 = XMVectorSet(cosf(t1) * radius, sinf(t1) * radius, 0.f, 1.f);
            }

            p0 = XMVector3Transform(p0, world);
            p1 = XMVector3Transform(p1, world);

            _matrix lineWorld = XMMatrixIdentity();
            if (!BuildLineWorldMatrix(p0, p1, lineWorld))
                continue;

            m_pLineMaterial->Bind_Matrix(lineWorld);
            m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
            m_pLineMesh->Render();
        }
    };

    drawCircle(0);
    drawCircle(1);
    drawCircle(2);
#endif
}

const _float CSphereCollider::GetRadius() const
{
    return m_fRadius;
}

void CSphereCollider::SetRadius(const _float radius)
{
    m_fRadius = max(radius, 0.001f);
    m_bShapeDirty = true;
}

void CSphereCollider::OnDestroy()
{
    Safe_Release(m_pLineMesh);
    Safe_Release(m_pLineMaterial);

    __super::OnDestroy();
}

void CSphereCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    ReleaseShape();

    const _float radius = max(m_fRadius, 0.001f);
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError())
    {
        m_bShapeDirty = false;
        return;
    }

    JPH::Ref<JPH::Shape> sphereRef = sphereResult.Get();
    const JPH::Shape* baseShape = sphereRef.GetPtr();
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
