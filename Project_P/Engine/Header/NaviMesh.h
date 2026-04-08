#pragma once

#include "MeshBuffer.h"
#include "DetourNavMesh.h"

class dtNavMeshQuery;

NS_BEGIN(Engine)

class CCamera;
class CMaterial;

NS_BEGIN(EngineAI)

class ENGINE_DLL CNaviMesh final : public CMeshBuffer
{
	friend class CResources;

public:
	struct NavBakeOptions
	{
		_float cellSize = 0.3f;
		_float cellHeight = 0.05f;
		_float agentHeight = 2.f;
		_float agentRadius = 0.6f;
		_float agentMaxClimb = 0.9f;
		_float agentMaxSlope = 45.f;
		_int regionMinSize = 8;
		_int regionMergeSize = 20;
		_float edgeMaxLen = 12.f;
		_float edgeMaxError = 1.3f;
		_int vertsPerPoly = 6;
		_float detailSampleDist = 6.f;
		_float detailSampleMaxError = 0.25f;
		vector3 queryHalfExtents = vector3(2.f, 4.f, 2.f);
		_bool rasterizeObstacleMeshes = true;
	};

	struct MeshSource
	{
		CMeshBuffer* meshBuffer = nullptr;
		_float4x4 worldMatrix = {};
		wstring label = L"";
		_bool walkable = true;
	};

	struct NaviPolygon
	{
		_uint index = 0;
		vector<_uint> neighbors = {};
		vector<vector3> vertices = {};
	};

	struct BoundaryEdge
	{
		vector3 start = vector3::zero();
		vector3 end = vector3::zero();
	};

	struct SerializedTileData
	{
		uint64_t tileRef = 0;
		vector<uint8_t> data = {};
	};

	struct SerializedNavMeshData
	{
		dtNavMeshParams params = {};
		vector<SerializedTileData> tiles = {};
	};

	struct PathNode
	{
		_uint polygonIndex = 0;
		float gCost = 0.f;
		float hCost = 0.f;
		PathNode* parent = nullptr;
	};

private:
	CNaviMesh();
	~CNaviMesh();

public:
	HRESULT BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes);
	HRESULT BuildFromSources(const vector<MeshSource>& _sources);
	bool FindPath(const vector3& _start, const vector3& _end, vector<vector3>& _outPath);
	bool SamplePosition(const vector3& _position, vector3& _outPoint, int* _outPolygonIndex = nullptr);
	bool ConstrainMovement(const vector3& _start, const vector3& _end, vector3& _outPoint, int* _outPolygonIndex = nullptr);
	int FindContainingPolygon(const vector3& _position);
	void SetBakeOptions(const NavBakeOptions& _options);
	const NavBakeOptions& GetBakeOptions() const;
	bool ExportSerializedNavMesh(SerializedNavMeshData& _outData) const;
	HRESULT ImportSerializedNavMesh(const SerializedNavMeshData& _data);
	const _bool IsBuilt() const;
	const vector<NaviPolygon>& GetPolygons() const;
	const vector<BoundaryEdge>& GetBoundaryEdges() const;
	const _bool HasRenderMesh() const;
	void RenderOverlay(CCamera* _camera, CMaterial* _material);
	static CNaviMesh* CreateRuntime(const wstring& _name = L"");

private:
	static CNaviMesh* Create();
	HRESULT InitializeQuery();
	void ReleaseNavigation();
	void RebuildPolygonCache();
	void ReleaseRenderMesh();
	HRESULT RebuildRenderMesh();

private:
	NavBakeOptions m_sBakeOptions;
	dtNavMesh* m_pNavMesh;
	dtNavMeshQuery* m_pNavMeshQuery;
	vector<NaviPolygon> m_vPolygons;
	vector<BoundaryEdge> m_vBoundaryEdges;
	vector<dtPolyRef> m_vPolygonRefs;
	unordered_map<dtPolyRef, _uint> m_mPolyRefToIndex;
};

NS_END

NS_END
