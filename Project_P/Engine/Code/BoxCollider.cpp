#include "epch.h"
#include "BoxCollider.h"
#include "Resources.h"
#include "MeshBuffer.h"
#include "Material.h"
#include "Editor.h"
#include "SceneManager.h"
#include "Camera.h"

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

CBoxCollider::CBoxCollider()
    : m_vSize(vector3::one())
    , m_pLineMesh(nullptr)
    , m_pLineMaterial(nullptr)
{
    m_strName = L"Box Collider";
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

void CBoxCollider::Update()
{
    __super::Update();
}

void CBoxCollider::FixedUpdate()
{
}

void CBoxCollider::Render_Editor()
{
}

void CBoxCollider::Render_Gizmo()
{
#ifndef _CLIENT_BUILD
    if (!m_pLineMesh || !m_pLineMaterial)
        return;

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    if (!cam)
        return;

    if (IsContacting())
        m_pLineMaterial->Set_BaseColor(_float4(1.f, 0.f, 0.f, 1.f));
    else
        m_pLineMaterial->Set_BaseColor(_float4(0.f, 1.f, 0.f, 1.f));

    const _float hx = m_vSize.x * 0.5f;
    const _float hy = m_vSize.y * 0.5f;
    const _float hz = m_vSize.z * 0.5f;

    _vector localCorners[8] =
    {
        XMVectorSet(-hx, -hy, -hz, 1.f),
        XMVectorSet(hx, -hy, -hz, 1.f),
        XMVectorSet(hx, hy, -hz, 1.f),
        XMVectorSet(-hx, hy, -hz, 1.f),
        XMVectorSet(-hx, -hy, hz, 1.f),
        XMVectorSet(hx, -hy, hz, 1.f),
        XMVectorSet(hx, hy, hz, 1.f),
        XMVectorSet(-hx, hy, hz, 1.f)
    };

    const _matrix objectWorld = Get_Transform()->Get_WorldMatrix();
    const _matrix centerOffset = XMMatrixTranslation(m_vCenter.x, m_vCenter.y, m_vCenter.z);
    const _matrix world = centerOffset * objectWorld;

    _vector worldCorners[8];
    for (_uint i = 0; i < 8; ++i)
        worldCorners[i] = XMVector3Transform(localCorners[i], world);

    constexpr _uint edges[12][2] =
    {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    _float3 camPos = _float3();
    _matrix matView = cam->Get_ViewMatrix();
    _matrix matProj = cam->Get_ProjectionMatrix();

    for (_uint i = 0; i < 12; ++i)
    {
        _matrix lineWorld = XMMatrixIdentity();
        if (!BuildLineWorldMatrix(worldCorners[edges[i][0]], worldCorners[edges[i][1]], lineWorld))
            continue;

        m_pLineMaterial->Bind_Matrix(lineWorld);
        m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
        m_pLineMesh->Render();
    }
#endif
}

const vector3& CBoxCollider::GetSize() const
{
    return m_vSize;
}

void CBoxCollider::SetSize(const vector3& size)
{
    m_vSize = size;
    m_bShapeDirty = true;
}

void CBoxCollider::OnDestroy()
{
    Safe_Release(m_pLineMesh);
    Safe_Release(m_pLineMaterial);

    __super::OnDestroy();
}

void CBoxCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    ReleaseShape();

    const vector3 scale = Get_Transform()->Get_LocalScale();
    const float sx = max(m_vSize.x * fabsf(scale.x), 0.001f);
    const float sy = max(m_vSize.y * fabsf(scale.y), 0.001f);
    const float sz = max(m_vSize.z * fabsf(scale.z), 0.001f);

    const JPH::Vec3 halfExtent(sx * 0.5f, sy * 0.5f, sz * 0.5f);

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
        const JPH::Vec3 center(m_vCenter.x * scale.x, m_vCenter.y * scale.y, m_vCenter.z * scale.z);

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
