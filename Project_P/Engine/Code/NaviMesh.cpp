#include "epch.h"
#include "NaviMesh.h"

#include "Camera.h"
#include "Material.h"

#include "DetourAlloc.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"
#include "Recast.h"

using namespace EngineAI;

namespace
{
	constexpr int kMinQueryNodeCount = 2048;
	constexpr int kMaxPathPolygonCount = 512;
	constexpr int kMaxStraightPathPointCount = 512;
	constexpr float kDegenerateTriangleEpsilon = 1e-6f;
	constexpr float kPolygonNormalEpsilon = 1e-6f;

	class CRecastBuildContext final : public rcContext
	{
	public:
		CRecastBuildContext()
			: rcContext(true)
		{
		}

	protected:
		void doLog(const rcLogCategory category, const char* msg, const int len) override
		{
			if (!msg || len <= 0)
				return;

			const string text(msg, msg + len);
			if (category == RC_LOG_ERROR)
				CDebug::LogError(text);
			else
				CDebug::Log(text);
		}
	};

	struct HeightFieldDeleter
	{
		void operator()(rcHeightfield* ptr) const
		{
			if (ptr)
				rcFreeHeightField(ptr);
		}
	};

	struct CompactHeightFieldDeleter
	{
		void operator()(rcCompactHeightfield* ptr) const
		{
			if (ptr)
				rcFreeCompactHeightfield(ptr);
		}
	};

	struct ContourSetDeleter
	{
		void operator()(rcContourSet* ptr) const
		{
			if (ptr)
				rcFreeContourSet(ptr);
		}
	};

	struct PolyMeshDeleter
	{
		void operator()(rcPolyMesh* ptr) const
		{
			if (ptr)
				rcFreePolyMesh(ptr);
		}
	};

	struct PolyMeshDetailDeleter
	{
		void operator()(rcPolyMeshDetail* ptr) const
		{
			if (ptr)
				rcFreePolyMeshDetail(ptr);
		}
	};

	using HeightFieldPtr = unique_ptr<rcHeightfield, HeightFieldDeleter>;
	using CompactHeightFieldPtr = unique_ptr<rcCompactHeightfield, CompactHeightFieldDeleter>;
	using ContourSetPtr = unique_ptr<rcContourSet, ContourSetDeleter>;
	using PolyMeshPtr = unique_ptr<rcPolyMesh, PolyMeshDeleter>;
	using PolyMeshDetailPtr = unique_ptr<rcPolyMeshDetail, PolyMeshDetailDeleter>;

	struct NavigationGeometry
	{
		vector<float> vertices = {};
		vector<int> indices = {};
		vector<unsigned char> areas = {};
	};

	unsigned char CalculateTriangleArea(const float* a, const float* b, const float* c, const float walkableThreshold)
	{
		const float abx = b[0] - a[0];
		const float aby = b[1] - a[1];
		const float abz = b[2] - a[2];
		const float acx = c[0] - a[0];
		const float acy = c[1] - a[1];
		const float acz = c[2] - a[2];

		const float nx = aby * acz - abz * acy;
		const float ny = abz * acx - abx * acz;
		const float nz = abx * acy - aby * acx;
		const float lengthSq = nx * nx + ny * ny + nz * nz;

		if (lengthSq <= kDegenerateTriangleEpsilon)
			return RC_NULL_AREA;

		const float invLength = 1.f / sqrtf(lengthSq);
		return (ny * invLength) >= walkableThreshold ? RC_WALKABLE_AREA : RC_NULL_AREA;
	}

	void BuildQueryHalfExtents(const CNaviMesh::NavBakeOptions& options, float* outHalfExtents)
	{
		outHalfExtents[0] = max(options.queryHalfExtents.x, max(options.agentRadius * 2.f, options.cellSize * 2.f));
		outHalfExtents[1] = max(options.queryHalfExtents.y, max(options.agentHeight, options.cellHeight * 2.f));
		outHalfExtents[2] = max(options.queryHalfExtents.z, max(options.agentRadius * 2.f, options.cellSize * 2.f));
	}

	void ToFloatArray(const vector3& value, float* outValue)
	{
		outValue[0] = value.x;
		outValue[1] = value.y;
		outValue[2] = value.z;
	}

	_bool IsZeroMatrix(const _float4x4& value)
	{
		const float* raw = reinterpret_cast<const float*>(&value);
		for (_uint i = 0; i < 16; ++i)
		{
			if (raw[i] != 0.f)
				return false;
		}

		return true;
	}

	_matrix LoadWorldMatrixOrIdentity(const _float4x4& value)
	{
		if (IsZeroMatrix(value))
			return XMMatrixIdentity();

		return XMLoadFloat4x4(&value);
	}

	void BuildPolygonBasis(const vector<vector3>& polygonVertices, _float3& outNormal, _float3& outTangent)
	{
		_vector normal = XMVectorZero();
		if (polygonVertices.size() >= 3)
		{
			const _vector origin = XMVectorSet(polygonVertices[0].x, polygonVertices[0].y, polygonVertices[0].z, 0.f);
			for (size_t vertexIndex = 1; vertexIndex + 1 < polygonVertices.size(); ++vertexIndex)
			{
				const _vector a = XMVectorSet(polygonVertices[vertexIndex].x, polygonVertices[vertexIndex].y, polygonVertices[vertexIndex].z, 0.f);
				const _vector b = XMVectorSet(polygonVertices[vertexIndex + 1].x, polygonVertices[vertexIndex + 1].y, polygonVertices[vertexIndex + 1].z, 0.f);
				const _vector cross = XMVector3Cross(a - origin, b - origin);
				if (XMVectorGetX(XMVector3LengthSq(cross)) > kPolygonNormalEpsilon)
				{
					normal = XMVector3Normalize(cross);
					break;
				}
			}
		}

		if (XMVectorGetX(XMVector3LengthSq(normal)) <= kPolygonNormalEpsilon)
			normal = XMVectorSet(0.f, 1.f, 0.f, 0.f);

		_vector tangent = XMVector3Cross(XMVectorSet(0.f, 1.f, 0.f, 0.f), normal);
		if (XMVectorGetX(XMVector3LengthSq(tangent)) <= kPolygonNormalEpsilon)
			tangent = XMVector3Cross(XMVectorSet(0.f, 0.f, 1.f, 0.f), normal);
		if (XMVectorGetX(XMVector3LengthSq(tangent)) <= kPolygonNormalEpsilon)
			tangent = XMVectorSet(1.f, 0.f, 0.f, 0.f);
		else
			tangent = XMVector3Normalize(tangent);

		XMStoreFloat3(&outNormal, normal);
		XMStoreFloat3(&outTangent, tangent);
	}

	constexpr float kNavigationBoundaryQuantizeScale = 1000.f;
	constexpr float kLegacyCellHeight = 0.2f;
	constexpr float kLegacyDetailSampleMaxError = 1.f;

	struct NavigationBoundaryVertexKey
	{
		_int x = 0;
		_int y = 0;
		_int z = 0;

		_bool operator==(const NavigationBoundaryVertexKey& _rhs) const
		{
			return x == _rhs.x && y == _rhs.y && z == _rhs.z;
		}
	};

	struct NavigationBoundaryEdgeKey
	{
		NavigationBoundaryVertexKey start = {};
		NavigationBoundaryVertexKey end = {};

		_bool operator==(const NavigationBoundaryEdgeKey& _rhs) const
		{
			return start == _rhs.start && end == _rhs.end;
		}
	};

	struct NavigationBoundaryEdgeKeyHasher
	{
		size_t operator()(const NavigationBoundaryEdgeKey& _key) const
		{
			size_t hashValue = 0;
			hash<_int> hasher = {};
			auto combine = [&](const _int _value)
				{
					hashValue ^= hasher(_value) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
				};

			combine(_key.start.x);
			combine(_key.start.y);
			combine(_key.start.z);
			combine(_key.end.x);
			combine(_key.end.y);
			combine(_key.end.z);
			return hashValue;
		}
	};

	struct NavigationBoundaryEdgeRecord
	{
		vector3 start = vector3::zero();
		vector3 end = vector3::zero();
		_uint count = 0;
	};

	_int QuantizeNavigationCoord(const _float _value)
	{
		return static_cast<_int>(roundf(_value * kNavigationBoundaryQuantizeScale));
	}

	NavigationBoundaryVertexKey MakeNavigationBoundaryVertexKey(const vector3& _point)
	{
		NavigationBoundaryVertexKey key = {};
		key.x = QuantizeNavigationCoord(_point.x);
		key.y = QuantizeNavigationCoord(_point.y);
		key.z = QuantizeNavigationCoord(_point.z);
		return key;
	}

	_bool IsLessNavigationBoundaryVertexKey(const NavigationBoundaryVertexKey& _lhs, const NavigationBoundaryVertexKey& _rhs)
	{
		if (_lhs.x != _rhs.x)
			return _lhs.x < _rhs.x;
		if (_lhs.y != _rhs.y)
			return _lhs.y < _rhs.y;
		return _lhs.z < _rhs.z;
	}

	NavigationBoundaryEdgeKey MakeNavigationBoundaryEdgeKey(const vector3& _start, const vector3& _end)
	{
		NavigationBoundaryEdgeKey key = {};
		key.start = MakeNavigationBoundaryVertexKey(_start);
		key.end = MakeNavigationBoundaryVertexKey(_end);
		if (IsLessNavigationBoundaryVertexKey(key.end, key.start))
			swap(key.start, key.end);
		return key;
	}

	void NormalizeBakeOptions(CNaviMesh::NavBakeOptions& _options)
	{
		const _bool isLegacyCellHeight = fabsf(_options.cellHeight - kLegacyCellHeight) <= 0.0001f;
		const _bool isLegacyDetailSampleMaxError = fabsf(_options.detailSampleMaxError - kLegacyDetailSampleMaxError) <= 0.0001f;
		if (isLegacyCellHeight && isLegacyDetailSampleMaxError)
		{
			_options.cellHeight = 0.05f;
			_options.detailSampleMaxError = 0.25f;
		}
	}
	bool AppendMeshGeometry(CMeshBuffer* mesh, const _float4x4& worldMatrix, const _bool markWalkable, const float walkableThreshold, NavigationGeometry& outGeometry, const wstring& label)
	{
		if (!mesh)
			return true;

		const CMeshBuffer::MESHBUFFERDESC& info = mesh->Get_Info();
		if (info.topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{
			CDebug::LogError(L"Navigation build failed - only triangle list meshes are supported: " + label);
			return false;
		}

		vector<VertexTexNormalTangentBuffer> vertices = mesh->Get_VertexBuffer();
		if (vertices.empty())
		{
			CDebug::LogError(L"Navigation build failed - mesh has no readable vertex buffer: " + label);
			return false;
		}

		vector<_uint> indices = mesh->Get_IndexBuffer();
		if (indices.empty())
		{
			if ((vertices.size() % 3) != 0)
			{
				CDebug::LogError(L"Navigation build failed - non indexed mesh is not divisible by 3: " + label);
				return false;
			}

			indices.reserve(vertices.size());
			for (_uint i = 0; i < vertices.size(); ++i)
				indices.push_back(i);
		}

		if ((indices.size() % 3) != 0)
		{
			CDebug::LogError(L"Navigation build failed - mesh index count is not divisible by 3: " + label);
			return false;
		}

		const size_t baseVertex = outGeometry.vertices.size() / 3;
		const _float scale = mesh->Get_ScaleFactor();
		const _matrix world = LoadWorldMatrixOrIdentity(worldMatrix);

		outGeometry.vertices.reserve(outGeometry.vertices.size() + vertices.size() * 3);
		for (const auto& vertex : vertices)
		{
			const _vector localPos = XMVectorSet(vertex.position.x * scale, vertex.position.y * scale, vertex.position.z * scale, 1.f);
			const _vector worldPos = XMVector3TransformCoord(localPos, world);
			_float3 transformed = {};
			XMStoreFloat3(&transformed, worldPos);
			outGeometry.vertices.push_back(transformed.x);
			outGeometry.vertices.push_back(transformed.y);
			outGeometry.vertices.push_back(transformed.z);
		}

		_int appendedTriangleCount = 0;
		for (size_t triIndex = 0; triIndex < indices.size(); triIndex += 3)
		{
			const int ia = static_cast<int>(indices[triIndex + 0]);
			const int ib = static_cast<int>(indices[triIndex + 1]);
			const int ic = static_cast<int>(indices[triIndex + 2]);

			if (ia < 0 || ib < 0 || ic < 0 ||
				ia >= static_cast<int>(vertices.size()) ||
				ib >= static_cast<int>(vertices.size()) ||
				ic >= static_cast<int>(vertices.size()))
			{
				continue;
			}

			const size_t aOffset = (baseVertex + static_cast<size_t>(ia)) * 3;
			const size_t bOffset = (baseVertex + static_cast<size_t>(ib)) * 3;
			const size_t cOffset = (baseVertex + static_cast<size_t>(ic)) * 3;

			const float a[3] =
			{
				outGeometry.vertices[aOffset + 0],
				outGeometry.vertices[aOffset + 1],
				outGeometry.vertices[aOffset + 2]
			};
			const float b[3] =
			{
				outGeometry.vertices[bOffset + 0],
				outGeometry.vertices[bOffset + 1],
				outGeometry.vertices[bOffset + 2]
			};
			const float c[3] =
			{
				outGeometry.vertices[cOffset + 0],
				outGeometry.vertices[cOffset + 1],
				outGeometry.vertices[cOffset + 2]
			};

			outGeometry.indices.push_back(static_cast<int>(baseVertex + static_cast<size_t>(ia)));
			outGeometry.indices.push_back(static_cast<int>(baseVertex + static_cast<size_t>(ib)));
			outGeometry.indices.push_back(static_cast<int>(baseVertex + static_cast<size_t>(ic)));
			outGeometry.areas.push_back(markWalkable ? CalculateTriangleArea(a, b, c, walkableThreshold) : RC_NULL_AREA);
			++appendedTriangleCount;
		}

		if (appendedTriangleCount == 0)
		{
			CDebug::LogError(L"Navigation build failed - mesh did not contribute any valid triangles: " + label);
			return false;
		}

		return true;
	}
}

CNaviMesh::CNaviMesh()
	: m_pNavMesh(nullptr)
	, m_pNavMeshQuery(nullptr)
{
}

CNaviMesh::~CNaviMesh()
{
	ReleaseNavigation();
}

HRESULT CNaviMesh::BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes)
{
	_float4x4 identity = {};
	XMStoreFloat4x4(&identity, XMMatrixIdentity());

	vector<MeshSource> sources = {};
	if (_sourceMesh)
	{
		MeshSource source = {};
		source.meshBuffer = _sourceMesh;
		source.worldMatrix = identity;
		source.label = L"source mesh";
		source.walkable = true;
		sources.push_back(source);
	}

	if (m_sBakeOptions.rasterizeObstacleMeshes)
	{
		for (CMeshBuffer* obstacleMesh : _obstacleMeshes)
		{
			if (!obstacleMesh || obstacleMesh == _sourceMesh)
				continue;

			MeshSource obstacle = {};
			obstacle.meshBuffer = obstacleMesh;
			obstacle.worldMatrix = identity;
			obstacle.label = L"obstacle mesh";
			obstacle.walkable = false;
			sources.push_back(obstacle);
		}
	}

	return BuildFromSources(sources);
}

HRESULT CNaviMesh::BuildFromSources(const vector<MeshSource>& _sources)
{
	ReleaseNavigation();

	if (_sources.empty())
	{
		CDebug::LogError(L"Navigation build failed - no mesh sources were provided.");
		return E_INVALIDARG;
	}

	if (m_sBakeOptions.cellSize <= 0.f || m_sBakeOptions.cellHeight <= 0.f ||
		m_sBakeOptions.agentHeight <= 0.f || m_sBakeOptions.agentRadius < 0.f ||
		m_sBakeOptions.agentMaxClimb < 0.f || m_sBakeOptions.vertsPerPoly < 3 ||
		m_sBakeOptions.vertsPerPoly > DT_VERTS_PER_POLYGON)
	{
		CDebug::LogError(L"Navigation build failed - invalid bake options.");
		return E_INVALIDARG;
	}

	NavigationGeometry geometry = {};
	const float clampedSlope = min(max(m_sBakeOptions.agentMaxSlope, 0.f), 89.9f);
	const float walkableThreshold = cosf(clampedSlope * (XM_PI / 180.f));
	_uint validSourceCount = 0;
	_uint walkableSourceCount = 0;

	for (const MeshSource& source : _sources)
	{
		if (!source.meshBuffer)
			continue;

		const wstring label = source.label.empty() ? source.meshBuffer->Get_ResourceName() : source.label;
		if (!AppendMeshGeometry(source.meshBuffer, source.worldMatrix, source.walkable, walkableThreshold, geometry, label))
			return E_FAIL;

		++validSourceCount;
		if (source.walkable)
			++walkableSourceCount;
	}

	const int vertexCount = static_cast<int>(geometry.vertices.size() / 3);
	const int triangleCount = static_cast<int>(geometry.indices.size() / 3);
	if (validSourceCount == 0 || vertexCount == 0 || triangleCount == 0)
	{
		CDebug::LogError(L"Navigation build failed - no geometry was collected.");
		return E_FAIL;
	}

	CRecastBuildContext buildContext;

	rcConfig config = {};
	config.cs = m_sBakeOptions.cellSize;
	config.ch = m_sBakeOptions.cellHeight;
	config.walkableSlopeAngle = clampedSlope;
	config.walkableHeight = max(1, static_cast<int>(ceilf(m_sBakeOptions.agentHeight / config.ch)));
	config.walkableClimb = max(0, static_cast<int>(floorf(m_sBakeOptions.agentMaxClimb / config.ch)));
	config.walkableRadius = max(0, static_cast<int>(ceilf(m_sBakeOptions.agentRadius / config.cs)));
	config.maxEdgeLen = max(0, static_cast<int>(floorf(m_sBakeOptions.edgeMaxLen / config.cs)));
	config.maxSimplificationError = max(0.1f, m_sBakeOptions.edgeMaxError);
	config.minRegionArea = max(0, m_sBakeOptions.regionMinSize * m_sBakeOptions.regionMinSize);
	config.mergeRegionArea = max(0, m_sBakeOptions.regionMergeSize * m_sBakeOptions.regionMergeSize);
	config.maxVertsPerPoly = m_sBakeOptions.vertsPerPoly;
	config.detailSampleDist = m_sBakeOptions.detailSampleDist < 0.9f ? 0.f : config.cs * m_sBakeOptions.detailSampleDist;
	config.detailSampleMaxError = max(0.f, config.ch * m_sBakeOptions.detailSampleMaxError);

	rcCalcBounds(geometry.vertices.data(), vertexCount, config.bmin, config.bmax);
	rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

	if (config.width <= 0 || config.height <= 0)
	{
		CDebug::LogError(L"Navigation build failed - invalid navigation bounds.");
		return E_FAIL;
	}

	HeightFieldPtr solid(rcAllocHeightfield());
	CompactHeightFieldPtr compactHeightField(rcAllocCompactHeightfield());
	ContourSetPtr contourSet(rcAllocContourSet());
	PolyMeshPtr polyMesh(rcAllocPolyMesh());
	PolyMeshDetailPtr detailMesh(rcAllocPolyMeshDetail());

	if (!solid || !compactHeightField || !contourSet || !polyMesh || !detailMesh)
	{
		CDebug::LogError(L"Navigation build failed - could not allocate Recast working buffers.");
		return E_OUTOFMEMORY;
	}

	if (!rcCreateHeightfield(&buildContext, *solid, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
	{
		CDebug::LogError(L"Navigation build failed - rcCreateHeightfield failed.");
		return E_FAIL;
	}

	if (!rcRasterizeTriangles(&buildContext, geometry.vertices.data(), vertexCount, geometry.indices.data(), geometry.areas.data(), triangleCount, *solid, config.walkableClimb))
	{
		CDebug::LogError(L"Navigation build failed - rcRasterizeTriangles failed.");
		return E_FAIL;
	}

	rcFilterLowHangingWalkableObstacles(&buildContext, config.walkableClimb, *solid);
	rcFilterLedgeSpans(&buildContext, config.walkableHeight, config.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(&buildContext, config.walkableHeight, *solid);

	if (!rcBuildCompactHeightfield(&buildContext, config.walkableHeight, config.walkableClimb, *solid, *compactHeightField))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildCompactHeightfield failed.");
		return E_FAIL;
	}

	if (config.walkableRadius > 0 && !rcErodeWalkableArea(&buildContext, config.walkableRadius, *compactHeightField))
	{
		CDebug::LogError(L"Navigation build failed - rcErodeWalkableArea failed.");
		return E_FAIL;
	}

	if (!rcBuildDistanceField(&buildContext, *compactHeightField))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildDistanceField failed.");
		return E_FAIL;
	}

	if (!rcBuildRegions(&buildContext, *compactHeightField, 0, config.minRegionArea, config.mergeRegionArea))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildRegions failed.");
		return E_FAIL;
	}

	if (!rcBuildContours(&buildContext, *compactHeightField, config.maxSimplificationError, config.maxEdgeLen, *contourSet))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildContours failed.");
		return E_FAIL;
	}

	if (!rcBuildPolyMesh(&buildContext, *contourSet, config.maxVertsPerPoly, *polyMesh))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildPolyMesh failed.");
		return E_FAIL;
	}

	if (!rcBuildPolyMeshDetail(&buildContext, *polyMesh, *compactHeightField, config.detailSampleDist, config.detailSampleMaxError, *detailMesh))
	{
		CDebug::LogError(L"Navigation build failed - rcBuildPolyMeshDetail failed.");
		return E_FAIL;
	}

	if (polyMesh->npolys == 0)
	{
		CDebug::LogError(L"Navigation build failed - no walkable polygons were generated.");
		return E_FAIL;
	}

	for (int polygonIndex = 0; polygonIndex < polyMesh->npolys; ++polygonIndex)
		polyMesh->flags[polygonIndex] = polyMesh->areas[polygonIndex] != RC_NULL_AREA ? 1 : 0;

	dtNavMeshCreateParams navMeshParams = {};
	navMeshParams.verts = polyMesh->verts;
	navMeshParams.vertCount = polyMesh->nverts;
	navMeshParams.polys = polyMesh->polys;
	navMeshParams.polyAreas = polyMesh->areas;
	navMeshParams.polyFlags = polyMesh->flags;
	navMeshParams.polyCount = polyMesh->npolys;
	navMeshParams.nvp = polyMesh->nvp;
	navMeshParams.detailMeshes = detailMesh->meshes;
	navMeshParams.detailVerts = detailMesh->verts;
	navMeshParams.detailVertsCount = detailMesh->nverts;
	navMeshParams.detailTris = detailMesh->tris;
	navMeshParams.detailTriCount = detailMesh->ntris;
	navMeshParams.walkableHeight = m_sBakeOptions.agentHeight;
	navMeshParams.walkableRadius = m_sBakeOptions.agentRadius;
	navMeshParams.walkableClimb = m_sBakeOptions.agentMaxClimb;
	navMeshParams.cs = config.cs;
	navMeshParams.ch = config.ch;
	navMeshParams.buildBvTree = true;
	rcVcopy(navMeshParams.bmin, polyMesh->bmin);
	rcVcopy(navMeshParams.bmax, polyMesh->bmax);

	unsigned char* navData = nullptr;
	int navDataSize = 0;
	if (!dtCreateNavMeshData(&navMeshParams, &navData, &navDataSize) || !navData || navDataSize <= 0)
	{
		CDebug::LogError(L"Navigation build failed - dtCreateNavMeshData failed.");
		return E_FAIL;
	}

	m_pNavMesh = dtAllocNavMesh();
	if (!m_pNavMesh)
	{
		dtFree(navData);
		CDebug::LogError(L"Navigation build failed - dtAllocNavMesh failed.");
		return E_OUTOFMEMORY;
	}

	if (dtStatusFailed(m_pNavMesh->init(navData, navDataSize, DT_TILE_FREE_DATA)))
	{
		dtFree(navData);
		dtFreeNavMesh(m_pNavMesh);
		m_pNavMesh = nullptr;
		CDebug::LogError(L"Navigation build failed - dtNavMesh::init failed.");
		return E_FAIL;
	}

	m_pNavMeshQuery = dtAllocNavMeshQuery();
	if (!m_pNavMeshQuery)
	{
		ReleaseNavigation();
		CDebug::LogError(L"Navigation build failed - dtAllocNavMeshQuery failed.");
		return E_OUTOFMEMORY;
	}

	const int queryNodeCount = max(kMinQueryNodeCount, polyMesh->npolys * 16);
	if (dtStatusFailed(m_pNavMeshQuery->init(m_pNavMesh, queryNodeCount)))
	{
		ReleaseNavigation();
		CDebug::LogError(L"Navigation build failed - dtNavMeshQuery::init failed.");
		return E_FAIL;
	}

	RebuildPolygonCache();
	if (FAILED(RebuildRenderMesh()))
		CDebug::LogError(L"Navigation mesh overlay build failed - the navmesh will still be usable for pathfinding.");

	CDebug::Log(L"Navigation mesh build complete: " + to_wstring(walkableSourceCount) + L" NavigationStatic meshes.");

	return S_OK;
}

bool CNaviMesh::FindPath(const vector3& _start, const vector3& _end, vector<vector3>& _outPath)
{
	_outPath.clear();

	if (!IsBuilt())
		return false;

	float startPos[3] = {};
	float endPos[3] = {};
	float halfExtents[3] = {};
	ToFloatArray(_start, startPos);
	ToFloatArray(_end, endPos);
	BuildQueryHalfExtents(m_sBakeOptions, halfExtents);

	dtQueryFilter filter = {};
	dtPolyRef startRef = 0;
	dtPolyRef endRef = 0;
	float nearestStart[3] = {};
	float nearestEnd[3] = {};

	if (dtStatusFailed(m_pNavMeshQuery->findNearestPoly(startPos, halfExtents, &filter, &startRef, nearestStart)) || !startRef)
		return false;

	if (dtStatusFailed(m_pNavMeshQuery->findNearestPoly(endPos, halfExtents, &filter, &endRef, nearestEnd)) || !endRef)
		return false;

	dtPolyRef polygonPath[kMaxPathPolygonCount] = {};
	int polygonPathCount = 0;
	if (dtStatusFailed(m_pNavMeshQuery->findPath(startRef, endRef, nearestStart, nearestEnd, &filter, polygonPath, &polygonPathCount, kMaxPathPolygonCount)) || polygonPathCount <= 0)
		return false;

	float straightPath[kMaxStraightPathPointCount * 3] = {};
	unsigned char straightPathFlags[kMaxStraightPathPointCount] = {};
	dtPolyRef straightPathRefs[kMaxStraightPathPointCount] = {};
	int straightPathCount = 0;

	if (dtStatusFailed(m_pNavMeshQuery->findStraightPath(nearestStart, nearestEnd, polygonPath, polygonPathCount, straightPath, straightPathFlags, straightPathRefs, &straightPathCount, kMaxStraightPathPointCount)) || straightPathCount <= 0)
		return false;

	_outPath.reserve(straightPathCount);
	for (int pointIndex = 0; pointIndex < straightPathCount; ++pointIndex)
	{
		const int baseIndex = pointIndex * 3;
		_outPath.emplace_back(straightPath[baseIndex + 0], straightPath[baseIndex + 1], straightPath[baseIndex + 2]);
	}

	return !_outPath.empty();
}

bool CNaviMesh::SamplePosition(const vector3& _position, vector3& _outPoint, int* _outPolygonIndex)
{
    _outPoint = _position;
    if (_outPolygonIndex)
        *_outPolygonIndex = -1;

    if (!IsBuilt())
        return false;

    float position[3] = {};
    float halfExtents[3] = {};
    float nearestPoint[3] = {};
    ToFloatArray(_position, position);
    BuildQueryHalfExtents(m_sBakeOptions, halfExtents);

    dtQueryFilter filter = {};
    dtPolyRef polygonRef = 0;
    if (dtStatusFailed(m_pNavMeshQuery->findNearestPoly(position, halfExtents, &filter, &polygonRef, nearestPoint)) || !polygonRef)
        return false;

    float closestPoint[3] = { nearestPoint[0], nearestPoint[1], nearestPoint[2] };
    bool isPointOverPolygon = false;
    if (dtStatusFailed(m_pNavMeshQuery->closestPointOnPoly(polygonRef, position, closestPoint, &isPointOverPolygon)))
        return false;

    float polygonHeight = closestPoint[1];
    if (dtStatusSucceed(m_pNavMeshQuery->getPolyHeight(polygonRef, isPointOverPolygon ? position : closestPoint, &polygonHeight)))
        closestPoint[1] = polygonHeight;

    _outPoint = vector3(closestPoint[0], closestPoint[1], closestPoint[2]);
    if (_outPolygonIndex)
    {
        const auto foundPolygon = m_mPolyRefToIndex.find(polygonRef);
        if (foundPolygon != m_mPolyRefToIndex.end())
            *_outPolygonIndex = static_cast<int>(foundPolygon->second);
    }

    return true;
}

int CNaviMesh::FindContainingPolygon(const vector3& _position)
{
	if (!IsBuilt())
		return -1;

	float position[3] = {};
	float halfExtents[3] = {};
	float nearestPoint[3] = {};
	ToFloatArray(_position, position);
	BuildQueryHalfExtents(m_sBakeOptions, halfExtents);

	dtQueryFilter filter = {};
	dtPolyRef polygonRef = 0;
	if (dtStatusFailed(m_pNavMeshQuery->findNearestPoly(position, halfExtents, &filter, &polygonRef, nearestPoint)) || !polygonRef)
		return -1;

	const auto foundPolygon = m_mPolyRefToIndex.find(polygonRef);
	if (foundPolygon == m_mPolyRefToIndex.end())
		return -1;

	return static_cast<int>(foundPolygon->second);
}

void CNaviMesh::SetBakeOptions(const NavBakeOptions& _options)
{
	m_sBakeOptions = _options;
	NormalizeBakeOptions(m_sBakeOptions);
}

const CNaviMesh::NavBakeOptions& CNaviMesh::GetBakeOptions() const
{
	return m_sBakeOptions;
}

const _bool CNaviMesh::IsBuilt() const
{
	return m_pNavMesh != nullptr && m_pNavMeshQuery != nullptr;
}

const vector<CNaviMesh::NaviPolygon>& CNaviMesh::GetPolygons() const
{
	return m_vPolygons;
}

const vector<CNaviMesh::BoundaryEdge>& CNaviMesh::GetBoundaryEdges() const
{
	return m_vBoundaryEdges;
}

const _bool CNaviMesh::HasRenderMesh() const
{
	return m_pVertexBuffer != nullptr && m_sInfo.vertextCount > 0;
}

void CNaviMesh::RenderOverlay(CCamera* _camera, CMaterial* _material)
{
	if (!_camera || !_material || !HasRenderMesh())
		return;

	_float3 cameraPosition = {};
	if (CTransform* cameraTransform = _camera->GetTransform())
	{
		const vector3 position = cameraTransform->Get_Position();
		cameraPosition = _float3(position.x, position.y, position.z);
	}

	_material->Bind_Matrix(XMMatrixIdentity());
	_material->Bind_Camera(cameraPosition, _camera->GetViewMatrix(), _camera->GetProjectionMatrix(), 0);
	Render();
}

CNaviMesh* CNaviMesh::CreateRuntime(const wstring& _name)
{
	CNaviMesh* navMesh = Create();
	if (!navMesh)
		return nullptr;

	navMesh->AddRef();
	if (FAILED(navMesh->Initialize(_name, L"", nullptr)))
	{
		Safe_Release(navMesh);
		return nullptr;
	}

	return navMesh;
}

CNaviMesh* CNaviMesh::Create()
{
	return new CNaviMesh();
}

void CNaviMesh::ReleaseNavigation()
{
	ReleaseRenderMesh();
	m_vPolygons.clear();
	m_vBoundaryEdges.clear();
	m_vPolygonRefs.clear();
	m_mPolyRefToIndex.clear();

	if (m_pNavMeshQuery)
	{
		dtFreeNavMeshQuery(m_pNavMeshQuery);
		m_pNavMeshQuery = nullptr;
	}

	if (m_pNavMesh)
	{
		dtFreeNavMesh(m_pNavMesh);
		m_pNavMesh = nullptr;
	}
}

void CNaviMesh::ReleaseRenderMesh()
{
	Safe_Release(m_pVertexBuffer);
	Safe_Release(m_pIndexBuffer);

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

	m_vBoundaryEdges.clear();
	m_sInfo = {};
}

HRESULT CNaviMesh::RebuildRenderMesh()
{
	ReleaseRenderMesh();

	if (!m_pNavMesh)
		return S_FALSE;

	vector<VertexSkinnedBuffer> vertices = {};
	vector<_uint> indices = {};
	unordered_map<NavigationBoundaryEdgeKey, NavigationBoundaryEdgeRecord, NavigationBoundaryEdgeKeyHasher> boundaryRecords = {};
	vertices.reserve(1024);
	indices.reserve(2048);

	_vector minBounds = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
	_vector maxBounds = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);

	auto appendPointBounds = [&](const vector3& point)
		{
			const _vector position = XMVectorSet(point.x, point.y, point.z, 1.f);
			minBounds = XMVectorMin(minBounds, position);
			maxBounds = XMVectorMax(maxBounds, position);
		};

	auto appendBoundaryEdge = [&](const vector3& start, const vector3& end)
		{
			if ((end - start).lengthSq() <= kDegenerateTriangleEpsilon)
				return;

			const NavigationBoundaryEdgeKey key = MakeNavigationBoundaryEdgeKey(start, end);
			NavigationBoundaryEdgeRecord& record = boundaryRecords[key];
			if (record.count == 0)
			{
				record.start = start;
				record.end = end;
			}

			++record.count;
		};

	auto getDetailVertex = [](const dtMeshTile* tile, const dtPoly* polygon, const dtPolyDetail& detail, unsigned char vertexIndex) -> vector3
		{
			if (vertexIndex < polygon->vertCount)
			{
				const float* vertex = &tile->verts[polygon->verts[vertexIndex] * 3];
				return vector3(vertex[0], vertex[1], vertex[2]);
			}

			const unsigned int detailVertexIndex = detail.vertBase + (vertexIndex - polygon->vertCount);
			const float* vertex = &tile->detailVerts[detailVertexIndex * 3];
			return vector3(vertex[0], vertex[1], vertex[2]);
		};

	const dtNavMesh* navMesh = m_pNavMesh;
	for (int tileIndex = 0; tileIndex < navMesh->getMaxTiles(); ++tileIndex)
	{
		const dtMeshTile* tile = navMesh->getTile(tileIndex);
		if (!tile || !tile->header || !tile->polys || !tile->detailMeshes || !tile->detailTris)
			continue;

		for (int polygonIndex = 0; polygonIndex < tile->header->polyCount; ++polygonIndex)
		{
			const dtPoly* polygon = &tile->polys[polygonIndex];
			if (!polygon || polygon->getType() != DT_POLYTYPE_GROUND)
				continue;

			const dtPolyDetail& detail = tile->detailMeshes[polygonIndex];
			for (unsigned int triangleIndex = 0; triangleIndex < detail.triCount; ++triangleIndex)
			{
				const unsigned char* triangle = &tile->detailTris[(detail.triBase + triangleIndex) * 4];
				const vector3 a = getDetailVertex(tile, polygon, detail, triangle[0]);
				const vector3 b = getDetailVertex(tile, polygon, detail, triangle[1]);
				const vector3 c = getDetailVertex(tile, polygon, detail, triangle[2]);

				vector3 normal = vector3::Cross(b - a, c - a);
				if (normal.lengthSq() <= kPolygonNormalEpsilon)
					normal = vector3::up();
				else
					normal = normal.normalized();

				vector3 tangent = vector3::Cross(vector3::up(), normal);
				if (tangent.lengthSq() <= kPolygonNormalEpsilon)
					tangent = vector3::right();
				else
					tangent = tangent.normalized();

				const _uint baseVertex = static_cast<_uint>(vertices.size());
				const vector3 trianglePoints[3] = { a, b, c };
				for (const vector3& point : trianglePoints)
				{
					VertexSkinnedBuffer vertex = {};
					vertex.position = _float3(point.x, point.y, point.z);
					vertex.normal = _float3(normal.x, normal.y, normal.z);
					vertex.uv = _float2(0.f, 0.f);
					vertex.tangent = _float3(tangent.x, tangent.y, tangent.z);
					vertices.push_back(vertex);
					appendPointBounds(point);
				}

				indices.push_back(baseVertex + 0);
				indices.push_back(baseVertex + 1);
				indices.push_back(baseVertex + 2);

				appendBoundaryEdge(a, b);
				appendBoundaryEdge(b, c);
				appendBoundaryEdge(c, a);
			}
		}
	}

	m_vBoundaryEdges.clear();
	m_vBoundaryEdges.reserve(boundaryRecords.size());
	for (const auto& boundaryRecord : boundaryRecords)
	{
		if (boundaryRecord.second.count != 1)
			continue;

		BoundaryEdge edge = {};
		edge.start = boundaryRecord.second.start;
		edge.end = boundaryRecord.second.end;
		m_vBoundaryEdges.push_back(edge);
	}

	if (vertices.empty() || indices.empty())
		return S_FALSE;

	CMeshBuffer::MeshBufferInitiaizeInfo info = {};
	info.meshName = m_strResourceName;
	info.buffer.assign(
		reinterpret_cast<const uint8_t*>(vertices.data()),
		reinterpret_cast<const uint8_t*>(vertices.data()) + sizeof(VertexSkinnedBuffer) * vertices.size());
	info.indices = indices;
	info.desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	info.desc.vertexSize = sizeof(VertexSkinnedBuffer);
	info.desc.vertextCount = static_cast<_uint>(vertices.size());
	info.desc.indexCount = static_cast<_uint>(indices.size());
	BoundingBox::CreateFromPoints(info.desc.boundingBox, minBounds, maxBounds);

	return Initialize_Custom(info, nullptr);
}
void CNaviMesh::RebuildPolygonCache()
{
	m_vPolygons.clear();
	m_vBoundaryEdges.clear();
	m_vPolygonRefs.clear();
	m_mPolyRefToIndex.clear();

	if (!m_pNavMesh)
		return;

	struct PolygonSource
	{
		const dtMeshTile* tile = nullptr;
		const dtPoly* polygon = nullptr;
		dtPolyRef reference = 0;
	};

	vector<PolygonSource> polygonSources = {};
	const dtNavMesh* navMesh = m_pNavMesh;
	for (int tileIndex = 0; tileIndex < navMesh->getMaxTiles(); ++tileIndex)
	{
		const dtMeshTile* tile = navMesh->getTile(tileIndex);
		if (!tile || !tile->header)
			continue;

		const dtPolyRef baseReference = m_pNavMesh->getPolyRefBase(tile);
		for (int polygonIndex = 0; polygonIndex < tile->header->polyCount; ++polygonIndex)
		{
			const dtPoly& polygon = tile->polys[polygonIndex];
			if (polygon.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;

			PolygonSource source = {};
			source.tile = tile;
			source.polygon = &polygon;
			source.reference = baseReference + static_cast<dtPolyRef>(polygonIndex);
			polygonSources.push_back(source);

			NaviPolygon cachedPolygon = {};
			cachedPolygon.index = static_cast<_uint>(m_vPolygons.size());
			cachedPolygon.vertices.reserve(polygon.vertCount);
			for (unsigned char vertexIndex = 0; vertexIndex < polygon.vertCount; ++vertexIndex)
			{
				const float* vertex = &tile->verts[polygon.verts[vertexIndex] * 3];
				cachedPolygon.vertices.emplace_back(vertex[0], vertex[1], vertex[2]);
			}

			m_mPolyRefToIndex[source.reference] = cachedPolygon.index;
			m_vPolygonRefs.push_back(source.reference);
			m_vPolygons.push_back(move(cachedPolygon));
		}
	}

	for (size_t polygonIndex = 0; polygonIndex < polygonSources.size(); ++polygonIndex)
	{
		const PolygonSource& source = polygonSources[polygonIndex];
		if (!source.tile || !source.polygon)
			continue;

		NaviPolygon& cachedPolygon = m_vPolygons[polygonIndex];
		for (unsigned int linkIndex = source.polygon->firstLink; linkIndex != DT_NULL_LINK; linkIndex = source.tile->links[linkIndex].next)
		{
			const dtPolyRef neighborReference = source.tile->links[linkIndex].ref;
			const auto neighbor = m_mPolyRefToIndex.find(neighborReference);
			if (neighbor == m_mPolyRefToIndex.end() || neighbor->second == cachedPolygon.index)
				continue;

			if (find(cachedPolygon.neighbors.begin(), cachedPolygon.neighbors.end(), neighbor->second) == cachedPolygon.neighbors.end())
				cachedPolygon.neighbors.push_back(neighbor->second);
		}
	}
}











