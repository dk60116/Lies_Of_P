#pragma once

#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMeshBuffer : public CEngineResource
{
	friend class CResources;
	friend class CSceneLoader;
	friend class CParticleSystem;
	friend class CPhysics;

public:
	typedef struct MeshBufferDescription
	{
		_bool useDeviceTopology = false;
		D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		_uint vertexSize = 0;
		_uint vertextCount = 0;
		_uint indexCount = 0;
	}MESHBUFFERDESC;

	struct MeshBufferInitiaizeInfo
	{
		wstring meshName = L"";
		vector<uint8_t> buffer = {};
		vector<_uint> indices = {};
		MESHBUFFERDESC desc = {};
		wstring diffuseMapPath = L"";
	};

	typedef struct TerrainMeshBufferDesctiption
	{
		_bool isHeightMapBase = false;
		_uint landscape = 100;
		_uint portrait = 100;
		_float size = 0.5f;
		_float heightWeight = 1.f;
		wstring heightMap = L"";
	}TERRAINBUFFERDESC;

protected:
	explicit CMeshBuffer();
	~CMeshBuffer();

private:
	static CMeshBuffer* Create();

protected:
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy();

public:
	HRESULT Initialize_Custom(MeshBufferInitiaizeInfo _info, void* _desc);

public:
	static wstring FindMeshName(const aiScene* scene, _uint meshIndex, aiNode* node = nullptr);

private:
	static MeshBufferInitiaizeInfo CreateLine();
	static MeshBufferInitiaizeInfo CreateLineRect();
	static MeshBufferInitiaizeInfo CreateRect();
	MeshBufferInitiaizeInfo CreateCube();
	MeshBufferInitiaizeInfo CreateSphere();
	MeshBufferInitiaizeInfo CreatePlane();
	MeshBufferInitiaizeInfo CreateQuad();
	MeshBufferInitiaizeInfo CreateCylinder();
	MeshBufferInitiaizeInfo CreateTriangle();
	MeshBufferInitiaizeInfo CreateTerrain(_uint _sizeX, _uint _sizeZ, const _float _size, _float _heighyWeight, ID3D11Texture2D* _heightMap);

public:
	void Render();

public:
	const _float Get_ScaleFactor() const;
	virtual void Set_Scalefactor(const _float _value);
	vector<VertexTexNormalTangentBuffer> Get_VertexBuffer() const;
	vector<_uint> Get_IndexBuffer() const;
	const MESHBUFFERDESC& Get_Info();

protected:
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;

	MESHBUFFERDESC m_sInfo;

protected:
	void* m_pVertexSysMem;
	void* m_pIndexSysMem;

	_float m_fScaleFactor;
};

NS_END

