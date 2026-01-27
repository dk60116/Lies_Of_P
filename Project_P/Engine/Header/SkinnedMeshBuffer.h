#pragma once

#include "MeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSkinnedMeshBuffer : public CMeshBuffer
{
	friend class CResources;
	friend class CSkinnedMeshRenderer;

public:
	struct SkinnedBufferInitiaizeInfo
	{
		wstring meshName = L"";
		vector<uint8_t> buffer = {};
		vector<_uint> indices = {};
		MESHBUFFERDESC desc = {};
		wstring diffuseMapPath = L"";
		vector<wstring> boneNames;
		vector<_float4x4> boneOffsetMatrices;
	};

	typedef struct SkinnedNodeInfo
	{
		_int nodeId = -1;
		wstring name = L"";
		_float4x4 transformation = {};
		_int parentId = -1;
		_uint numChild = 0;
		vector<_int> childsId = {};
		_uint numMeshes = 0;
		vector<_uint> meshsId = {};
	}SKINNEDSKELETAL;

	struct SkinnedBuffer
	{
		vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> initList = {};
		vector<SKINNEDSKELETAL> skeletalList = {};
	};

private:
	explicit CSkinnedMeshBuffer();
	~CSkinnedMeshBuffer();

private:
	static CSkinnedMeshBuffer* Create();

protected:
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	HRESULT Initiailize_Custom(SkinnedBufferInitiaizeInfo _info, vector<SKINNEDSKELETAL> _bonesInfo, void* _desc);
	void Render();
	void OnDestroy() override;

public:
	const _uint Get_BoneCount() const;
	const wstring& Get_BoneName(const _uint _index) const;

public:
	static void FillBoneWeights(VertexSkinnedBuffer& _targetBuffer, const _uint _index, const _float _weight);

private:
	static void FillBoneWeightsAndIndices(const aiMesh* mesh, vector<VertexSkinnedBuffer>& vertices);
	const _float4x4& Get_BoneOffsetMatrix(const _uint _index);

private:
	vector<wstring> m_vBoneNames;
	vector<_float4x4> m_vBoneOffsetMatrices;
};

NS_END