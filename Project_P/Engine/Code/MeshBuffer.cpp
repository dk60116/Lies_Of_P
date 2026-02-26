#include "epch.h"
#include "MeshBuffer.h"
#include "SkinnedMeshBuffer.h"

CMeshBuffer::CMeshBuffer()
	: m_pVertexBuffer(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_sInfo({})
    , m_pVertexSysMem(nullptr)
    , m_pIndexSysMem(nullptr)
    , m_fScaleFactor(1.f)
{
    m_strName = L"Mesh Buffer";
}

CMeshBuffer::~CMeshBuffer()
{
	OnDestroy();
}

CMeshBuffer* CMeshBuffer::Create()
{
    return new CMeshBuffer();
}

HRESULT CMeshBuffer::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
    if (FAILED(__super::Initialize(_name, _filePath, _desc)))
        return E_FAIL;

    MeshBufferInitiaizeInfo info = {};

    if (_filePath == L"../Assets/Line")
        info = CreateLine();
    else if (_filePath == L"../Assets/Rect")
        info = CreateRect();
    else if (_filePath == L"../Assets/LineRect")
        info = CreateLineRect();
    else if (_filePath == L"../Assets/Cube")
        info = CreateCube();
    else if (_filePath == L"../Assets/Sphere")
        info = CreateSphere();
    else if (_filePath == L"../Assets/Cylinder")
        info = CreateCylinder();
    else if (_filePath == L"../Assets/Quad")
        info = CreateQuad();
    else if (_filePath == L"../Assets/Terrain")
    {
        TERRAINBUFFERDESC terrainDesc = {};

        if (_desc)
            terrainDesc = *reinterpret_cast<TERRAINBUFFERDESC*>(_desc);

        if (terrainDesc.isHeightMapBase)
        {
            CTexture* heightMap = CResources::GetInstance().LoadOnScene<CTexture>(terrainDesc.heightMap);

            if (heightMap)
                info = CreateTerrain(terrainDesc.landscape, terrainDesc.portrait, terrainDesc.size, terrainDesc.heightWeight, heightMap->Get_Texture());
            else
                CDebug::LogError(L"Failded create terrain mesh buffer - Height map texture not found: " + terrainDesc.heightMap);
        }
        else
            info = CreateTerrain(terrainDesc.landscape, terrainDesc.portrait, 0, 0, nullptr);
    }
    else
        return S_OK;

    if (!(info.buffer.size() > 0))
        return E_FAIL;

    if (info.desc.vertexSize == 0 || info.desc.vertextCount == 0)
        return E_FAIL;

    m_sInfo = info.desc;

    size_t size = info.desc.vertexSize * info.desc.vertextCount;

    m_pVertexSysMem = malloc(size);
    memcpy(m_pVertexSysMem, info.buffer.data(), size);

    if (info.desc.indexCount > 0 && !info.indices.empty())
    {
        size_t indexSize = sizeof(_uint) * info.desc.indexCount;
        m_pIndexSysMem = malloc(indexSize);
        memcpy(m_pIndexSysMem, info.indices.data(), indexSize);
    }

    ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

    // VertexBuffer 
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = static_cast<_uint>(info.desc.vertexSize * info.desc.vertextCount);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = info.buffer.data();

    HRESULT hr = S_OK;

    hr = device->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);

    // IndexBuffer 
    if (info.desc.indexCount > 0 && info.indices.size() > 0)
    {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(_uint) * info.desc.indexCount;
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = info.indices.data();

        hr = device->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
    }

    if (FAILED(hr))
    {
        CDebug::LogError(L"Assimp MeshBuffer load failed: " + m_strFilePath);
        return E_FAIL;
    }

    return hr;
}

HRESULT CMeshBuffer::Initialize_Custom(MeshBufferInitiaizeInfo _info, void* _desc)
{
    if (!(_info.buffer.size() > 0))
        return E_FAIL;

    if (_info.desc.vertexSize == 0 || _info.desc.vertextCount == 0)
        return E_FAIL;

    m_sInfo = {};
    m_sInfo = _info.desc;

    m_strResourceName = _info.meshName;

    size_t size = _info.desc.vertexSize * _info.desc.vertextCount;

    m_pVertexSysMem = malloc(size);
    memcpy(m_pVertexSysMem, _info.buffer.data(), size);

    if (_info.desc.indexCount > 0 && !_info.indices.empty())
    {
        size_t indexSize = sizeof(_uint) * _info.desc.indexCount;
        m_pIndexSysMem = malloc(indexSize);
        memcpy(m_pIndexSysMem, _info.indices.data(), indexSize);
    }

    ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

    // VertexBuffer 
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = static_cast<_uint>(_info.desc.vertexSize * _info.desc.vertextCount);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = _info.buffer.data();

    HRESULT hr = S_OK;

    hr = device->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);

    // IndexBuffer 
    if (_info.desc.indexCount > 0 && _info.indices.size() > 0)
    {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(_uint) * _info.desc.indexCount;
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = _info.indices.data();

        hr = device->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
    }

    if (FAILED(hr))
    {
        CDebug::LogError(L"Assimp MeshBuffer load failed: " + m_strFilePath);
        return E_FAIL;
    }

    return hr;
}

wstring CMeshBuffer::FindMeshName(const aiScene* scene, _uint meshIndex, aiNode* node)
{
    if (!node)
        node = scene->mRootNode;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        if (node->mMeshes[i] == meshIndex)
            return CEngineString::StringToWString(node->mName.C_Str());
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        wstring result = FindMeshName(scene, meshIndex, node->mChildren[i]);
        if (!result.empty())
            return result;
    }

    return L"";
}

void CMeshBuffer::OnDestroy()
{
	if (m_pVertexSysMem)
	{
		free(m_pVertexSysMem);
		m_pVertexSysMem = nullptr;
	}

	if (m_pIndexSysMem)
	{
		free(m_pIndexSysMem);
		m_pIndexSysMem = nullptr;
	}
}

void CMeshBuffer::Render()
{
    if (!m_pVertexBuffer)
    {
        CDebug::LogError("Mesh buffer failed render - No vertex buffer");
        return;
    }

    _uint stride = m_sInfo.vertexSize;
    _uint offset = 0;

    CGraphicDevice::GetInstance().Get_Context()->IASetVertexBuffers
    (
        0, 1, &m_pVertexBuffer, &stride, &offset
    );

    if (m_pIndexBuffer)
        CGraphicDevice::GetInstance().Get_Context()->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    if (!m_sInfo.useDeviceTopology)
        CGraphicDevice::GetInstance().Get_Context()->IASetPrimitiveTopology(m_sInfo.topology);

    if (m_pIndexBuffer)
        CGraphicDevice::GetInstance().Get_Context()->DrawIndexed(m_sInfo.indexCount, 0, 0);
    else
        CGraphicDevice::GetInstance().Get_Context()->Draw(m_sInfo.vertextCount, 0);
}

void CMeshBuffer::Render_Instanced(const _uint _instanceCount)
{
    if (!m_pVertexBuffer || _instanceCount == 0)
        return;

    _uint stride = m_sInfo.vertexSize;
    _uint offset = 0;

    CGraphicDevice::GetInstance().Get_Context()->IASetVertexBuffers
    (
        0, 1, &m_pVertexBuffer, &stride, &offset
    );

    if (m_pIndexBuffer)
        CGraphicDevice::GetInstance().Get_Context()->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    if (!m_sInfo.useDeviceTopology)
        CGraphicDevice::GetInstance().Get_Context()->IASetPrimitiveTopology(m_sInfo.topology);

    if (m_pIndexBuffer)
        CGraphicDevice::GetInstance().Get_Context()->DrawIndexedInstanced(m_sInfo.indexCount, _instanceCount, 0, 0, 0);
    else
        CGraphicDevice::GetInstance().Get_Context()->DrawInstanced(m_sInfo.vertextCount, _instanceCount, 0, 0);
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateLine()
{
    MeshBufferInitiaizeInfo info = {};

    const _float length = 0.5f;

    VertexTexNormalTangentBuffer lineVertices[2] =
    {
        {{-length, 0, 0}, { 0, 0, 0}, {0, 0}, {0, 0, 0}},
        {{ length, 0, 0}, { 0, 0, 0}, {0, 0}, {0, 0, 0}}
    };

    CMeshBuffer::MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    desc.vertexSize = sizeof(VertexTexNormalTangentBuffer);
    desc.vertextCount = _countof(lineVertices);
    desc.indexCount = 0;
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, 0.f, 0.f);

    info.buffer.assign(reinterpret_cast<uint8_t*>(lineVertices), reinterpret_cast<uint8_t*>(lineVertices) + sizeof(lineVertices));

    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateLineRect()
{
    MeshBufferInitiaizeInfo info = {};

    const _float length = 0.5f;

    VertexTexNormalTangentBuffer rectVertices[8]
    {
        {{-length, length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ length, length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ length, length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ length, -length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ length, -length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ -length, -length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ -length, -length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }},
        {{ -length, length, 0}, { 0, 0, 0 }, { 0, 0 }, { 0, 0, 0 }}
    };

    CMeshBuffer::MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    desc.vertexSize = sizeof(VertexTexNormalTangentBuffer);
    desc.vertextCount = _countof(rectVertices);
    desc.indexCount = 0;
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, length, 0.f);

    info.buffer.assign(reinterpret_cast<uint8_t*>(rectVertices), reinterpret_cast<uint8_t*>(rectVertices) + sizeof(rectVertices));
    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateRect()
{
    MeshBufferInitiaizeInfo info = {};

    const _float length = 0.5f;

    VertexTexColorBuffer quadVertices[4] =
    {
            {{-length, -length, 0}, {0, 1}},
            {{ length, -length, 0}, {1, 1}},
            {{ length,  length, 0}, {1, 0}},
            {{-length,  length, 0}, {0, 0}}
    };

    static _uint quadIndices[6] =
    {
        2,1,0, 3,2,0
    };

    CMeshBuffer::MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    desc.vertexSize = sizeof(VertexTexColorBuffer);
    desc.vertextCount = _countof(quadVertices);
    desc.indexCount = _countof(quadIndices);
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, length, 0.f);

    info.buffer.assign(reinterpret_cast<uint8_t*>(quadVertices), reinterpret_cast<uint8_t*>(quadVertices) + sizeof(quadVertices));
    info.indices.assign(begin(quadIndices), end(quadIndices));
    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateCube()
{
    MeshBufferInitiaizeInfo info = {};

    const _float length = 0.5f;

    const _float offset = 0.001f;

    const _float f000 = 0.f;
    const _float f025 = 0.25f + offset;
    const _float f050 = 0.5f - offset;
    const _float f075 = 0.75f;
    const _float f100 = 1.0f;

    const _float f033 = 1.f / 3.f + offset;
    const _float f066 = 2.f / 3.f - offset;

    VertexTexNormalTangentBuffer cubeVertices[24] =
    {
            // (-Z)
            {{-length, -length, -length}, { 0,  0, -1}, {f100, f066}, {1, 0, 0}},
            {{ length, -length, -length}, { 0,  0, -1}, {f075, f066}, {1, 0, 0}},
            {{ length,  length, -length}, { 0,  0, -1}, {f075, f033}, {1, 0, 0}},
            {{-length,  length, -length}, { 0,  0, -1}, {f100, f033}, {1, 0, 0}},

            // (+Z)                                   
            {{ length, -length,  length}, { 0,  0,  1}, {f050, f066}, {-1 ,0 ,0}},
            {{-length, -length,  length}, { 0,  0,  1}, {f025, f066}, {-1 ,0 ,0}},
            {{-length,  length,  length}, { 0,  0,  1}, {f025, f033}, {-1 ,0 ,0}},
            {{ length,  length,  length}, { 0,  0,  1}, {f050, f033}, {-1 ,0 ,0}},

            // (-X)                                   
            {{-length, -length,  length}, {-1,  0,  0}, {f025, f066}, {0, 0, -1}},
            {{-length, -length, -length}, {-1,  0,  0}, {f000, f066}, {0, 0, -1}},
            {{-length,  length, -length}, {-1,  0,  0}, {f000, f033}, {0, 0, -1}},
            {{-length,  length,  length}, {-1,  0,  0}, {f025, f033}, {0, 0, -1}},

            // (+X)                                   
            {{ length, -length, -length}, { 1,  0,  0}, {f075, f066}, {0, 0, 1}},
            {{ length, -length,  length}, { 1,  0,  0}, {f050, f066}, {0, 0, 1}},
            {{ length,  length,  length}, { 1,  0,  0}, {f050, f033}, {0, 0, 1}},
            {{ length,  length, -length}, { 1,  0,  0}, {f075, f033}, {0, 0, 1}},

            // (+Y)                    
            {{-length,  length, -length}, { 0,  1,  0}, {f025, f000}, {1, 0, 0}},
            {{ length,  length, -length}, { 0,  1,  0}, {f050, f000}, {1, 0, 0}},
            {{ length,  length,  length}, { 0,  1,  0}, {f050, f033}, {1, 0, 0}},
            {{-length,  length,  length}, { 0,  1,  0}, {f025, f033}, {1, 0, 0}},

            // Ʒ(-Y)                      
            {{-length, -length,  length}, { 0, -1,  0}, {f025, f066}, {1, 0, 0}},
            {{ length, -length,  length}, { 0, -1,  0}, {f050, f066}, {1, 0, 0}},
            {{ length, -length, -length}, { 0, -1,  0}, {f050, f100}, {1, 0, 0}},
            {{-length, -length, -length}, { 0, -1,  0}, {f025, f100}, {1, 0, 0}},
    };

    static _uint cubeIndices[36] =
    {
        2,1,0, 3,2,0,   // 
        6,5,4, 7,6,4,   // 
        10,9,8,11,10,8,  // 
        14,13,12,15,14,12,// 
        18,17,16,19,18,16,// 
        22,21,20,23,22,20 // Ʒ
    };

    CMeshBuffer::MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    desc.vertexSize = sizeof(VertexTexNormalTangentBuffer);
    desc.vertextCount = _countof(cubeVertices);
    desc.indexCount = _countof(cubeIndices);
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, length, length);

    info.buffer.assign(reinterpret_cast<uint8_t*>(cubeVertices), reinterpret_cast<uint8_t*>(cubeVertices) + sizeof(cubeVertices));
    info.indices.assign(begin(cubeIndices), end(cubeIndices));
    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateSphere()
{
    MeshBufferInitiaizeInfo result = {};
    return result;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateQuad()
{
    MeshBufferInitiaizeInfo info = {};

    const _float length = 0.5f;

    VertexTexNormalTangentBuffer quadVertices[4] =
    {
            {{-length, -length, 0}, { 0,  0, -1}, {0, 1}, {1, 0, 0}},
            {{ length, -length, 0}, { 0,  0, -1}, {1, 1}, {1, 0, 0}},
            {{ length,  length, 0}, { 0,  0, -1}, {1, 0}, {1, 0, 0}},
            {{-length,  length, 0}, { 0,  0, -1}, {0, 0}, {1, 0, 0}},
    };

    static _uint quadIndices[6] =
    {
        2,1,0, 3,2,0
    };

    CMeshBuffer::MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    desc.vertexSize = sizeof(VertexTexNormalTangentBuffer);
    desc.vertextCount = _countof(quadVertices);
    desc.indexCount = _countof(quadIndices);
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, length, 0.f);

    info.buffer.assign(reinterpret_cast<uint8_t*>(quadVertices),reinterpret_cast<uint8_t*>(quadVertices) + sizeof(quadVertices));
    info.indices.assign(begin(quadIndices), end(quadIndices));
    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateTriangle()
{
    MeshBufferInitiaizeInfo info = {};

    using VTX = VertexTexNormalTangentBuffer;

    const _float length = 0.5f;

    VTX triVerts[3] =
    {
        //  
        {
            {0.f, length, 0.f},       // Position
            {0.f, 0.f, -1.f},         // Normal
            {0.5f, 0.f},              // UV
            {1.f, 0.f, 0.f}           // Tangent
        },
        //  Ʒ 
        {
            {length, -length, 0.f},
            {0.f, 0.f, -1.f},
            {1.f, 1.f},
            {1.f, 0.f, 0.f}
        },
        //  Ʒ 
        {
            {-length, -length, 0.f},
            {0.f, 0.f, -1.f},
            {0.f, 1.f},
            {1.f, 0.f, 0.f}
        }
    };

    // ε (0-1-2)
    _uint triIndices[3] = { 0, 1, 2 };

    info.buffer.assign(reinterpret_cast<uint8_t*>(triVerts), reinterpret_cast<uint8_t*>(triVerts) + sizeof(triVerts));
    info.indices.assign(begin(triIndices), end(triIndices));

    MESHBUFFERDESC desc{};
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    desc.vertexSize = sizeof(VTX);
    desc.vertextCount = _countof(triVerts);
    desc.indexCount = _countof(triIndices);
    desc.boundingBox.Center = _float3(0.f, 0.f, 0.f);
    desc.boundingBox.Extents = _float3(length, length, 0.f);

    info.desc = desc;

    return info;
}

CMeshBuffer::MeshBufferInitiaizeInfo CMeshBuffer::CreateTerrain(_uint _sizeX, _uint _sizeZ, const _float _scale, _float _heighyWeight, ID3D11Texture2D* _heightMap)
{
    _sizeX = max<_uint>(_sizeX, 1);
    _sizeZ = max<_uint>(_sizeZ, 1);

    const _uint vertCountX = _sizeX + 1;
    const _uint vertCountZ = _sizeZ + 1;
    const _uint totalVerts = vertCountX * vertCountZ;
    const _uint totalQuads = _sizeX * _sizeZ;
    const _uint totalIndices = totalQuads * 6;

    const _float startX = -static_cast<_float>(_sizeX) * _scale;
    const _float startZ = static_cast<_float>(_sizeZ) * _scale;

    using VTX = VertexTexNormalTangentBuffer;
    vector<VTX>  vertices(totalVerts);
    vector<_uint> indices;
    indices.reserve(totalIndices);

    vector<uint8_t> heightPixels;
    _uint hmWidth = 0, hmHeight = 0, hmStride = 0;

    if (_heightMap)
    {
        ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
        ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

        D3D11_TEXTURE2D_DESC hDesc{};
        _heightMap->GetDesc(&hDesc);
        hmWidth = hDesc.Width;
        hmHeight = hDesc.Height;

        D3D11_TEXTURE2D_DESC sDesc = hDesc;
        sDesc.BindFlags = 0;
        sDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sDesc.Usage = D3D11_USAGE_STAGING;
        ID3D11Texture2D* staging = nullptr;
        device->CreateTexture2D(&sDesc, nullptr, &staging);
        context->CopyResource(staging, _heightMap);

        D3D11_MAPPED_SUBRESOURCE m{};
        context->Map(staging, 0, D3D11_MAP_READ, 0, &m);
        hmStride = static_cast<_uint>(m.RowPitch);
        heightPixels.assign(static_cast<uint8_t*>(m.pData), static_cast<uint8_t*>(m.pData) + m.RowPitch * hmHeight);
        context->Unmap(staging, 0);

        context->Flush();
        Safe_Release(_heightMap);
        staging->Release();
        staging = nullptr;
    }

    auto SampleHeight = [&](_float u, _float v)->_float
        {
            if (heightPixels.empty())
                return 0.f;
            _uint x = static_cast<_uint>(clamp(u, 0.f, 1.f) * (hmWidth - 1));
            _uint y = static_cast<_uint>(clamp(v, 0.f, 1.f) * (hmHeight - 1));
            const _uint bytesPerPixel = 4;
            uint8_t* row = &heightPixels[y * hmStride];
            uint8_t  gray = row[x * bytesPerPixel + 0];
            return (gray / 255.f) * _heighyWeight;
        };

    auto idx = [vertCountX](_uint x, _uint z) { return z * vertCountX + x; };

    for (_uint z = 0; z < vertCountZ; ++z)
    {
        const _float v = static_cast<float>(z) / _sizeZ;
        for (_uint x = 0; x < vertCountX; ++x)
        {
            const _float u = static_cast<float>(x) / _sizeX;
            const _float h = SampleHeight(u, v);

            VTX& vert = vertices[idx(x, z)];
            vert.position = { startX + x * _scale,  h,  startZ - z * _scale };
            vert.normal = { 0, 0, 0 };
            vert.uv = { u, v };
            vert.tangent = { 1, 0, 0 };
        }
    }

    for (_uint z = 0; z < _sizeZ; ++z)
    {
        for (_uint x = 0; x < _sizeX; ++x)
        {
            _uint v0 = idx(x, z);
            _uint v1 = idx(x + 1, z);
            _uint v2 = idx(x, z + 1);
            _uint v3 = idx(x + 1, z + 1);

            indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
            indices.push_back(v1); indices.push_back(v3); indices.push_back(v2);
        }
    }

    vector<_vector> normals(vertices.size(), XMVectorZero());

    for (_uint i = 0; i < indices.size(); i += 3)
    {
        _uint i0 = indices[i + 0];
        _uint i1 = indices[i + 1];
        _uint i2 = indices[i + 2];

        auto& p0 = vertices[i0].position;
        auto& p1 = vertices[i1].position;
        auto& p2 = vertices[i2].position;

        _vector v0 = XMLoadFloat3(&p0);
        _vector v1 = XMLoadFloat3(&p1);
        _vector v2 = XMLoadFloat3(&p2);

        _vector edge1 = XMVectorSubtract(v1, v0);
        _vector edge2 = XMVectorSubtract(v2, v0);
        _vector faceNormal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        normals[i0] = XMVectorAdd(normals[i0], faceNormal);
        normals[i1] = XMVectorAdd(normals[i1], faceNormal);
        normals[i2] = XMVectorAdd(normals[i2], faceNormal);
    }

    for (_uint i = 0; i < vertices.size(); ++i)
    {
        _vector n = XMVector3Normalize(normals[i]);
        XMStoreFloat3(&vertices[i].normal, n);
    }

    MeshBufferInitiaizeInfo info{};
    info.buffer.assign(reinterpret_cast<const uint8_t*>(vertices.data()),
        reinterpret_cast<const uint8_t*>(vertices.data()) + sizeof(VTX) * vertices.size());
    info.indices.assign(indices.begin(), indices.end());

    info.desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    info.desc.vertexSize = sizeof(VTX);
    info.desc.vertextCount = static_cast<_uint>(vertices.size());
    info.desc.indexCount = static_cast<_uint>(indices.size());

    return info;
}

const CMeshBuffer::MESHBUFFERDESC& CMeshBuffer::Get_Info()
{
	return m_sInfo;
}

const _float CMeshBuffer::Get_ScaleFactor() const
{
    return m_fScaleFactor;
}

void CMeshBuffer::Set_Scalefactor(const _float _value)
{
    m_fScaleFactor = _value;
}

vector<VertexTexNormalTangentBuffer> CMeshBuffer::Get_VertexBuffer() const
{
    vector<VertexTexNormalTangentBuffer> result;
    if (!m_pVertexSysMem || m_sInfo.vertexSize != sizeof(VertexTexNormalTangentBuffer))
        return result;

    const _uint count = m_sInfo.vertextCount;
    auto* verts = static_cast<VertexTexNormalTangentBuffer*>(m_pVertexSysMem);

    result.assign(verts, verts + count);
    return result;
}

vector<_uint> CMeshBuffer::Get_IndexBuffer() const
{
    vector<_uint> result;

    if (!m_pIndexSysMem || m_sInfo.indexCount == 0)
        return result;

    _uint* pIndices = static_cast<_uint*>(m_pIndexSysMem);
    result.assign(pIndices, pIndices + m_sInfo.indexCount);

    return result;
}
