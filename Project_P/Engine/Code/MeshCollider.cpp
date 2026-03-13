#include "epch.h"
#include "MeshCollider.h"
#include "MeshRenderer.h"
#include "MeshBuffer.h"
#include "Resources.h"
#include "Material.h"
#include "SceneManager.h"
#include "Camera.h"
#include "GraphicDevice.h"
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace
{
    struct MeshShapeCacheKey
    {
        const CMeshBuffer* mesh = nullptr;
        int sx = 0;
        int sy = 0;
        int sz = 0;
        int meshScale = 0;

        _bool operator==(const MeshShapeCacheKey& rhs) const
        {
            return mesh == rhs.mesh
                && sx == rhs.sx
                && sy == rhs.sy
                && sz == rhs.sz
                && meshScale == rhs.meshScale;
        }
    };

    struct MeshShapeCacheKeyHasher
    {
        size_t operator()(const MeshShapeCacheKey& key) const
        {
            const size_t h0 = hash<const CMeshBuffer*>()(key.mesh);
            const size_t h1 = hash<int>()(key.sx);
            const size_t h2 = hash<int>()(key.sy);
            const size_t h3 = hash<int>()(key.sz);
            const size_t h4 = hash<int>()(key.meshScale);

            size_t h = h0;
            h ^= h1 + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= h2 + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= h3 + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= h4 + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    unordered_map<MeshShapeCacheKey, JPH::RefConst<JPH::Shape>, MeshShapeCacheKeyHasher> g_MeshShapeCache;

    int QuantizeScale(_float v)
    {
        return static_cast<int>(roundf(v * 10000.f));
    }



    vector3 ExtractWorldScale(CTransform* transform)
    {
        if (!transform)
            return vector3::one();

        XMVECTOR scaleVec;
        XMVECTOR rotVec;
        XMVECTOR transVec;
        XMMatrixDecompose(&scaleVec, &rotVec, &transVec, transform->Get_WorldMatrix());

        _float3 scale;
        XMStoreFloat3(&scale, scaleVec);
        return vector3(scale.x, scale.y, scale.z);
    }
    MeshShapeCacheKey MakeCacheKey(const CMeshBuffer* meshBuffer, const vector3& scale, const _float meshScale)
    {
        MeshShapeCacheKey key;
        key.mesh = meshBuffer;
        key.sx = QuantizeScale(scale.x);
        key.sy = QuantizeScale(scale.y);
        key.sz = QuantizeScale(scale.z);
        key.meshScale = QuantizeScale(meshScale);
        return key;
    }

    const JPH::Shape* BuildOrGetCachedMeshShape(CMeshBuffer* meshBuffer, const vector3& scale, const _float meshScale)
    {
        if (!meshBuffer)
            return nullptr;

        const MeshShapeCacheKey cacheKey = MakeCacheKey(meshBuffer, scale, meshScale);
        auto cacheIt = g_MeshShapeCache.find(cacheKey);
        if (cacheIt != g_MeshShapeCache.end())
        {
            const JPH::Shape* cachedShape = cacheIt->second.GetPtr();
            if (cachedShape)
                cachedShape->AddRef();
            return cachedShape;
        }

        vector<VertexTexNormalTangentBuffer> vertices = meshBuffer->Get_VertexBuffer();
        if (vertices.size() < 3)
            return nullptr;

        vector<_uint> indices = meshBuffer->Get_IndexBuffer();

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
            return nullptr;

        JPH::MeshShapeSettings meshSettings(triangles);
        JPH::ShapeSettings::ShapeResult meshResult = meshSettings.Create();

        if (meshResult.HasError())
            return nullptr;

        JPH::RefConst<JPH::Shape> meshRef = meshResult.Get();
        g_MeshShapeCache.insert({ cacheKey, meshRef });

        const JPH::Shape* builtShape = meshRef.GetPtr();
        if (builtShape)
            builtShape->AddRef();
        return builtShape;
    }
}

CMeshCollider::CMeshCollider()
    : m_pCachedMeshBuffer(nullptr)
    , m_pLineMaterial(nullptr)
    , m_bShowGizmo(true)
    , m_vCachedWorldScale(vector3::one())
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
    clone->m_bShowGizmo = m_bShowGizmo;
    clone->m_vCachedWorldScale = m_vCachedWorldScale;

    return clone;
}

HRESULT CMeshCollider::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

#ifndef _CLIENT_BUILD
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

    NotifyShapeChanged();

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
        NotifyShapeChanged();
    }

    const vector3 worldScale = GetWorldScale();
    const _float dx = fabsf(worldScale.x - m_vCachedWorldScale.x);
    const _float dy = fabsf(worldScale.y - m_vCachedWorldScale.y);
    const _float dz = fabsf(worldScale.z - m_vCachedWorldScale.z);

    if (dx > 0.0001f || dy > 0.0001f || dz > 0.0001f)
    {
        m_vCachedWorldScale = worldScale;
        NotifyShapeChanged();
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
    CEditor& editor = CEditor::GetInstance();
    if (!editor.IsMeshColliderGizmoVisible())
        return;

    if (!editor.IsColliderGizmoVisible() && editor.Get_SelectedGameObject() != m_pGameObject)
        return;

    if (!m_bShowGizmo)
        return;

    if (!m_pLineMaterial)
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

    m_pLineMaterial->Set_BaseColor(GetGizmoColor());

    const vector3 scale = GetWorldScale();
    const _float meshScale = renderer->GetScaleFactor();
    const _matrix centerOffset = XMMatrixTranslation(m_vCenter.x * scale.x * meshScale, m_vCenter.y * scale.y * meshScale, m_vCenter.z * scale.z * meshScale);
    const _matrix gizmoWorld = centerOffset * Get_Transform()->Get_WorldMatrix();

    _float3 camPos = _float3();
    _matrix matView = cam->GetViewMatrix();
    _matrix matProj = cam->GetProjectionMatrix();

    ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
    if (!context)
        return;

    ID3D11RasterizerState* wireframeRasterizer = CGraphicDevice::GetInstance().Get_Rasterizer_Wireframe();

    ID3D11RasterizerState* prevRasterizer = nullptr;
    if (wireframeRasterizer)
    {
        context->RSGetState(&prevRasterizer);
        context->RSSetState(wireframeRasterizer);
    }

    m_pLineMaterial->Bind_Matrix(gizmoWorld);
    m_pLineMaterial->Bind_Camera(camPos, matView, matProj, 0);
    meshBuffer->Render();

    if (wireframeRasterizer)
    {
        context->RSSetState(prevRasterizer);
        Safe_Release(prevRasterizer);
    }
#endif
}

void CMeshCollider::OnDestroy()
{
    Safe_Release(m_pLineMaterial);

    __super::OnDestroy();
}


const _bool CMeshCollider::IsGizmoVisible() const
{
    return m_bShowGizmo;
}

void CMeshCollider::SetGizmoVisible(const _bool visible)
{
    m_bShowGizmo = visible;
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

    const vector3 scale = GetWorldScale();
    m_vCachedWorldScale = scale;
    const _float meshScale = renderer->GetScaleFactor();

    const JPH::Shape* baseShape = BuildOrGetCachedMeshShape(meshBuffer, scale, meshScale);
    if (!baseShape)
    {
        m_bShapeDirty = false;
        return;
    }

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



