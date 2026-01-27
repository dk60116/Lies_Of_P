#include "epch.h"
#include "SkinnedMeshBuffer.h"

CSkinnedMeshBuffer::CSkinnedMeshBuffer()
    : CMeshBuffer{}
    , m_vBoneNames({})
    , m_vBoneOffsetMatrices({})
{
    m_strName = L"Skinned Mesh Buffer";
}

CSkinnedMeshBuffer::~CSkinnedMeshBuffer()
{
    OnDestroy();
}

CSkinnedMeshBuffer* CSkinnedMeshBuffer::Create()
{
    return new CSkinnedMeshBuffer();
}

HRESULT CSkinnedMeshBuffer::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
    if (FAILED(CEngineResource::Initialize(_name, _filePath, _desc)))
        return E_FAIL;

    return S_OK;
}

HRESULT CSkinnedMeshBuffer::Initiailize_Custom(SkinnedBufferInitiaizeInfo _info, vector<SKINNEDSKELETAL> _bonesInfo, void* _desc)
{
    if (_info.buffer.empty() ||
        _info.desc.vertexSize == 0 ||
        _info.desc.vertextCount == 0)
        return E_FAIL;

    m_sInfo = _info.desc;
    m_strResourceName = _info.meshName;

    const size_t vtxBytes = _info.desc.vertexSize *
        _info.desc.vertextCount;

    m_pVertexSysMem = std::malloc(vtxBytes);
    std::memcpy(m_pVertexSysMem, _info.buffer.data(), vtxBytes);

    if (_info.desc.indexCount &&
        !_info.indices.empty())
    {
        const size_t idxBytes = sizeof(_uint) *
            _info.desc.indexCount;
        m_pIndexSysMem = std::malloc(idxBytes);
        std::memcpy(m_pIndexSysMem,
            _info.indices.data(),
            idxBytes);
    }

    ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
    HRESULT hr = S_OK;

    {
        D3D11_BUFFER_DESC   bd{};
        D3D11_SUBRESOURCE_DATA sd{};
        bd.ByteWidth = static_cast<_uint>(vtxBytes);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        sd.pSysMem = _info.buffer.data();
        hr = device->CreateBuffer(&bd, &sd, &m_pVertexBuffer);
        if (FAILED(hr)) goto BufferFail;
    }

    if (_info.desc.indexCount &&
        !_info.indices.empty())
    {
        D3D11_BUFFER_DESC   bd{};
        D3D11_SUBRESOURCE_DATA sd{};
        bd.ByteWidth = sizeof(_uint) * _info.desc.indexCount;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        sd.pSysMem = _info.indices.data();
        hr = device->CreateBuffer(&bd, &sd, &m_pIndexBuffer);
        if (FAILED(hr)) goto BufferFail;
    }

    m_vBoneNames = _info.boneNames;      
    m_vBoneOffsetMatrices = _info.boneOffsetMatrices; // 스킨 본 offset

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity,
        DirectX::XMMatrixIdentity());

    for (const auto& node : _bonesInfo)
    {
        const auto& name = node.name;

        // 이미 존재하면 패스
        if (find(m_vBoneNames.begin(),
            m_vBoneNames.end(),
            name) != m_vBoneNames.end())
            continue;

        // 이름 추가
        m_vBoneNames.push_back(name);

        // 오프셋 행렬:
        //  - 스킨 가중치 없는 본이므로 단위행렬이면 충분
        m_vBoneOffsetMatrices.push_back(identity);
    }
    return S_OK;

BufferFail:
    CDebug::LogError(L"SkinnedBuffer load failed(Custom): " + m_strFilePath);
    return hr;
}

void CSkinnedMeshBuffer::Render()
{
    if (!m_pVertexBuffer)
        return;

    ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

    _uint stride = m_sInfo.vertexSize;
    _uint offset = 0;

    context->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

    if (m_pIndexBuffer)
    {
        context->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(m_sInfo.topology);
        context->DrawIndexed(m_sInfo.indexCount, 0, 0);
    }
    else
    {
        context->IASetPrimitiveTopology(m_sInfo.topology);
        context->Draw(m_sInfo.vertextCount, 0);
    }
}

void CSkinnedMeshBuffer::OnDestroy()
{
    __super::OnDestroy();

    m_vBoneNames.clear();
}

const _uint CSkinnedMeshBuffer::Get_BoneCount() const
{
    return static_cast<_uint>(m_vBoneNames.size());
}

const wstring& CSkinnedMeshBuffer::Get_BoneName(const _uint _index) const
{
    return m_vBoneNames[_index];
}

void CSkinnedMeshBuffer::FillBoneWeights(VertexSkinnedBuffer& _targetBuffer, const _uint _index, const _float _weight)
{
    for (_uint i = 0; i < 4; ++i)
    {
        if (_targetBuffer.boneWeights[i] == 0.f)
        {
            _targetBuffer.boneIndices[i] = _index;
            _targetBuffer.boneWeights[i] = _weight;
            return;
        }
    }

    // 4개 꽉 찼다면 가장 작은 weight을 대체
    _uint minIndex = 0;
    for (int i = 1; i < 4; ++i)
    {
        if (_targetBuffer.boneWeights[i] < _targetBuffer.boneWeights[minIndex])
            minIndex = i;
    }

    if (_targetBuffer.boneWeights[minIndex] < _weight)
    {
        _targetBuffer.boneIndices[minIndex] = _index;
        _targetBuffer.boneWeights[minIndex] = _weight;
    }
}

const _float4x4& CSkinnedMeshBuffer::Get_BoneOffsetMatrix(const _uint _index)
{
    return m_vBoneOffsetMatrices[_index];
}

void CSkinnedMeshBuffer::FillBoneWeightsAndIndices(const aiMesh* mesh, vector<VertexSkinnedBuffer>& vertices)
{
    // 본 인덱스/가중치 할당
    for (_uint i = 0; i < mesh->mNumBones; ++i)
    {
        const aiBone* bone = mesh->mBones[i];

        for (_uint j = 0; j < bone->mNumWeights; ++j)
        {
            const aiVertexWeight& vw = bone->mWeights[j];

            _uint vertexId = vw.mVertexId;
            _float weight = vw.mWeight;

            auto& v = vertices[vertexId];

            for (_uint k = 0; k < 4; ++k)
            {
                if (v.boneWeights[k] == 0.0f)
                {
                    v.boneIndices[k] = i;
                    v.boneWeights[k] = weight;
                    break;
                }
            }
        }
    }

    // 각 버텍스의 가중치를 큰 순서로 정렬 + 인덱스 함께 정렬
    for (auto& v : vertices)
    {
        // 가중치와 인덱스를 쌍으로 모음
        vector<pair<_uint, float>> bonePairs;
        for (int k = 0; k < 4; ++k)
        {
            if (v.boneWeights[k] > 0.0f)
                bonePairs.emplace_back(v.boneIndices[k], v.boneWeights[k]);
        }

        // 큰 가중치 순으로 정렬
        sort(bonePairs.begin(), bonePairs.end(),
            [](const pair<_uint, float>& a, const pair<_uint, float>& b)
            {
                return a.second > b.second;
            });

        // 다시 배열에 복사
        for (_uint k = 0; k < 4; ++k)
        {
            if (k < bonePairs.size())
            {
                v.boneIndices[k] = bonePairs[k].first;
                v.boneWeights[k] = bonePairs[k].second;
            }
            else
            {
                v.boneIndices[k] = 0;
                v.boneWeights[k] = 0.0f;
            }
        }

        // 정규화
        _float sum = v.boneWeights[0] + v.boneWeights[1] + v.boneWeights[2] + v.boneWeights[3];
        if (sum > 0.0f)
        {
            for (_uint k = 0; k < 4; ++k)
                v.boneWeights[k] /= sum;
        }
    }
}