#include "epch.h"
#include "MeshCollider.h"
#include "MeshRenderer.h"
#include "MeshBuffer.h"
#include "Resources.h"
#include "Material.h"
#include "SceneManager.h"
#include "Camera.h"
#include <unordered_set>
#include <cstdint>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

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

    uint64_t MakeEdgeKey(_uint a, _uint b)
    {
        const _uint minIdx = min(a, b);
        const _uint maxIdx = max(a, b);
        return (static_cast<uint64_t>(minIdx) << 32) | static_cast<uint64_t>(maxIdx);
    }
}

CMeshCollider::CMeshCollider()
    : m_pCachedMeshBuffer(nullptr)
    , m_pLineMesh(nullptr)
    , m_pLineMaterial(nullptr)
{
    m_strName = L"Mesh Collider";
}

CMeshCollider::~CMeshCollider()
{
}

CMeshCollider* CMeshCollider::Create()
{
    return new CMeshCollider();
}

CComponent* CMeshCollider::Clone() const
{
    CMeshCollider* clone = new CMeshCollider();

    clone->m_bIsTrigger = m_bIsTrigger;
    clone->m_vCenter = m_vCenter;
    clone->m_bShapeDirty = true;

    return clone;
}

HRESULT CMeshCollider::Initialize()
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

    m_bShapeDirty = true;

    return S_OK;
}

void CMeshCollider::Update()
{
    __super::Update();

    CMeshRenderer* renderer = Get_GameObject()->GetComponent<CMeshRenderer>();
    CMeshBuffer* currentMesh = renderer ? renderer->Get_MeshBuffer() : nullptr;

    if (m_pCachedMeshBuffer != currentMesh)
    {
        m_pCachedMeshBuffer = currentMesh;
        m_bShapeDirty = true;

        if (m_pRigidBody)
            m_pRigidBody->MarkBodyDirty();
    }
}

void CMeshCollider::FixedUpdate()
{
}

void CMeshCollider::Render_Editor()
{
}

void CMeshCollider::Render_Gizmo()
{
#ifndef _CLIENT_BUILD
    if (!m_pLineMesh || !m_pLineMaterial)
        return;

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    if (!cam)
        return;

    CMeshRenderer* renderer = Get_GameObject()->GetComponent<CMeshRenderer>();
    if (!renderer)
        return;

    CMeshBuffer* meshBuffer = renderer->Get_MeshBuffer();
    if (!meshBuffer)
        return;

    vector<VertexTexNormalTangentBuffer> vertices = meshBuffer->Get_VertexBuffer();
    if (vertices.size() < 3)
        return;

    vector<_uint> indices = meshBuffer->Get_IndexBuffer();

    if (IsContacting())
        m_pLineMaterial->Set_BaseColor(_float4(1.f, 0.f, 0.f, 1.f));
    else
        m_pLineMaterial->Set_BaseColor(_float4(0.f, 1.f, 0.f, 1.f));

    const vector3 scale = Get_Transform()->Get_LocalScale();
    const _float meshScale = renderer->GetScaleFactor();
    const vector3 center = vector3(m_vCenter.x * scale.x * meshScale, m_vCenter.y * scale.y * meshScale, m_vCenter.z * scale.z * meshScale);

    const _matrix world = Get_Transform()->Get_WorldMatrix();
    _vector s = XMVectorZero();
    _vector r = XMQuaternionIdentity();
    _vector t = XMVectorZero();
    XMMatrixDecompose(&s, &r, &t, world);
    const _matrix worldNoScale = XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(t);

    _float3 camPos = _float3();
    _matrix matView = cam->Get_ViewMatrix();
    _matrix matProj = cam->Get_ProjectionMatrix();

    unordered_set<uint64_t> visitedEdges;

    auto drawEdge = [&](_uint ia, _uint ib)
    {
        if (ia >= vertices.size() || ib >= vertices.size())
            return;

        const uint64_t edgeKey = MakeEdgeKey(ia, ib);
        if (visitedEdges.find(edgeKey) != visitedEdges.end())
            return;

        visitedEdges.insert(edgeKey);

        const _float3& pa = vertices[ia].position;
        const _float3& pb = vertices[ib].position;

        _vector a = XMVectorSet(pa.x * scale.x * meshScale + center.x, pa.y * scale.y * meshScale + center.y, pa.z * scale.z * meshScale + center.z, 1.f);
        _vector b = XMVectorSet(pb.x * scale.x * meshScale + center.x, pb.y * scale.y * meshScale + center.y, pb.z * scale.z * meshScale + center.z, 1.f);

        a = XMVector3Transform(a, worldNoScale);
        b = XMVector3Transform(b, worldNoScale);

        _matrix lineWorld = XMMatrixIdentity();
        if (!BuildLineWorldMatrix(a, b, lineWorld))
            return;

        m_pLineMaterial->Bind_Matrix(lineWorld);
        m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
        m_pLineMesh->Render();
    };

    if (indices.size() >= 3)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const _uint i0 = indices[i];
            const _uint i1 = indices[i + 1];
            const _uint i2 = indices[i + 2];

            drawEdge(i0, i1);
            drawEdge(i1, i2);
            drawEdge(i2, i0);
        }
    }
    else
    {
        for (size_t i = 0; i + 2 < vertices.size(); i += 3)
        {
            drawEdge(static_cast<_uint>(i), static_cast<_uint>(i + 1));
            drawEdge(static_cast<_uint>(i + 1), static_cast<_uint>(i + 2));
            drawEdge(static_cast<_uint>(i + 2), static_cast<_uint>(i));
        }
    }
#endif
}

void CMeshCollider::OnDestroy()
{
    Safe_Release(m_pLineMesh);
    Safe_Release(m_pLineMaterial);

    __super::OnDestroy();
}

void CMeshCollider::BuildShapeIfNeeded()
{
    if (!m_bShapeDirty && m_pShape != nullptr)
        return;

    ReleaseShape();

    CMeshRenderer* renderer = Get_GameObject()->GetComponent<CMeshRenderer>();
    if (!renderer)
    {
        m_pCachedMeshBuffer = nullptr;
        m_bShapeDirty = false;
        return;
    }

    CMeshBuffer* meshBuffer = renderer->Get_MeshBuffer();
    m_pCachedMeshBuffer = meshBuffer;

    if (!meshBuffer)
    {
        m_bShapeDirty = false;
        return;
    }

    vector<VertexTexNormalTangentBuffer> vertices = meshBuffer->Get_VertexBuffer();
    if (vertices.size() < 3)
    {
        m_bShapeDirty = false;
        return;
    }

    vector<_uint> indices = meshBuffer->Get_IndexBuffer();

    const vector3 scale = Get_Transform()->Get_LocalScale();
    const _float meshScale = renderer->GetScaleFactor();

    JPH::TriangleList triangles;

    if (indices.size() >= 3)
    {
        triangles.reserve(indices.size() / 3);

        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const _uint i0 = indices[i];
            const _uint i1 = indices[i + 1];
            const _uint i2 = indices[i + 2];

            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;

            const _float3& p0 = vertices[i0].position;
            const _float3& p1 = vertices[i1].position;
            const _float3& p2 = vertices[i2].position;

            triangles.emplace_back(
                JPH::Vec3(p0.x * scale.x * meshScale, p0.y * scale.y * meshScale, p0.z * scale.z * meshScale),
                JPH::Vec3(p1.x * scale.x * meshScale, p1.y * scale.y * meshScale, p1.z * scale.z * meshScale),
                JPH::Vec3(p2.x * scale.x * meshScale, p2.y * scale.y * meshScale, p2.z * scale.z * meshScale));
        }
    }
    else
    {
        triangles.reserve(vertices.size() / 3);

        for (size_t i = 0; i + 2 < vertices.size(); i += 3)
        {
            const _float3& p0 = vertices[i].position;
            const _float3& p1 = vertices[i + 1].position;
            const _float3& p2 = vertices[i + 2].position;

            triangles.emplace_back(
                JPH::Vec3(p0.x * scale.x * meshScale, p0.y * scale.y * meshScale, p0.z * scale.z * meshScale),
                JPH::Vec3(p1.x * scale.x * meshScale, p1.y * scale.y * meshScale, p1.z * scale.z * meshScale),
                JPH::Vec3(p2.x * scale.x * meshScale, p2.y * scale.y * meshScale, p2.z * scale.z * meshScale));
        }
    }

    if (triangles.empty())
    {
        m_bShapeDirty = false;
        return;
    }

    JPH::MeshShapeSettings meshSettings(triangles);
    JPH::ShapeSettings::ShapeResult meshResult = meshSettings.Create();

    if (meshResult.HasError())
    {
        m_bShapeDirty = false;
        return;
    }

    JPH::Ref<JPH::Shape> meshRef = meshResult.Get();
    const JPH::Shape* baseShape = meshRef.GetPtr();
    baseShape->AddRef();

    const _bool centerIsZero =
        (m_vCenter.x == 0.f && m_vCenter.y == 0.f && m_vCenter.z == 0.f);

    if (!centerIsZero)
    {
        const JPH::Vec3 center(m_vCenter.x * scale.x * meshScale, m_vCenter.y * scale.y * meshScale, m_vCenter.z * scale.z * meshScale);

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
