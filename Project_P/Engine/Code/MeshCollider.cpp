#include "epch.h"
#include "MeshCollider.h"
#include "MeshRenderer.h"
#include "MeshBuffer.h"
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

CMeshCollider::CMeshCollider()
    : m_pCachedMeshBuffer(nullptr)
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
}

void CMeshCollider::OnDestroy()
{
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
