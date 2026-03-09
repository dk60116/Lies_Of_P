#include "epch.h"
#include "NaviMesh.h"

using namespace EngineAI;

namespace
{
	struct QuantizedVertexKey
	{
		long long x = 0;
		long long y = 0;
		long long z = 0;

		_bool operator==(const QuantizedVertexKey& rhs) const
		{
			return x == rhs.x && y == rhs.y && z == rhs.z;
		}
	};

	struct QuantizedEdgeKey
	{
		QuantizedVertexKey a = {};
		QuantizedVertexKey b = {};

		_bool operator==(const QuantizedEdgeKey& rhs) const
		{
			return a == rhs.a && b == rhs.b;
		}
	};

	struct QuantizedEdgeKeyHash
	{
		size_t operator()(const QuantizedEdgeKey& key) const
		{
			const size_t hx0 = hash<long long>{}(key.a.x);
			const size_t hx1 = hash<long long>{}(key.a.y);
			const size_t hx2 = hash<long long>{}(key.a.z);
			const size_t hx3 = hash<long long>{}(key.b.x);
			const size_t hx4 = hash<long long>{}(key.b.y);
			const size_t hx5 = hash<long long>{}(key.b.z);
			return (((((hx0 * 1315423911u) ^ hx1) * 1315423911u) ^ hx2) * 1315423911u ^ hx3) * 1315423911u ^ hx4 ^ (hx5 << 1);
		}
	};

	QuantizedVertexKey MakeVertexKey(const vector3& point, const _float tolerance)
	{
		QuantizedVertexKey key = {};
		key.x = llround(point.x / tolerance);
		key.y = llround(point.y / tolerance);
		key.z = llround(point.z / tolerance);
		return key;
	}

	QuantizedEdgeKey MakeEdgeKey(const vector3& a, const vector3& b, const _float tolerance)
	{
		QuantizedEdgeKey key = {};
		key.a = MakeVertexKey(a, tolerance);
		key.b = MakeVertexKey(b, tolerance);
		if (key.b.x < key.a.x ||
			(key.b.x == key.a.x && key.b.y < key.a.y) ||
			(key.b.x == key.a.x && key.b.y == key.a.y && key.b.z < key.a.z))
		{
			swap(key.a, key.b);
		}
		return key;
	}

	vector3 ComputeTriangleCenter(const vector<vector3>& vertices)
	{
		if (vertices.size() < 3)
			return vector3::zero();

		return (vertices[0] + vertices[1] + vertices[2]) / 3.f;
	}

	_bool IsDegenerateTriangle(const vector3& a, const vector3& b, const vector3& c)
	{
		return vector3::Cross(b - a, c - a).lengthSq() <= 1e-6f;
	}

	_bool TrySampleTriangleHeightAtXZ(const vector<vector3>& vertices, const vector3& point, _float& outY)
	{
		if (vertices.size() < 3)
			return false;

		const _float ax = vertices[0].x;
		const _float az = vertices[0].z;
		const _float bx = vertices[1].x;
		const _float bz = vertices[1].z;
		const _float cx = vertices[2].x;
		const _float cz = vertices[2].z;
		const _float px = point.x;
		const _float pz = point.z;
		const _float denom = ((bz - cz) * (ax - cx)) + ((cx - bx) * (az - cz));
		if (fabsf(denom) <= 1e-6f)
			return false;

		const _float w0 = (((bz - cz) * (px - cx)) + ((cx - bx) * (pz - cz))) / denom;
		const _float w1 = (((cz - az) * (px - cx)) + ((ax - cx) * (pz - cz))) / denom;
		const _float w2 = 1.f - w0 - w1;
		constexpr _float baryTolerance = 0.025f;
		if (w0 < -baryTolerance || w1 < -baryTolerance || w2 < -baryTolerance)
			return false;

		outY = vertices[0].y * w0 + vertices[1].y * w1 + vertices[2].y * w2;
		return true;
	}

	_float ComputePointToSegmentDistanceSqXZ(const vector3& point, const vector3& a, const vector3& b)
	{
		const vector3 ab = vector3(b.x - a.x, 0.f, b.z - a.z);
		const vector3 ap = vector3(point.x - a.x, 0.f, point.z - a.z);
		const _float abLenSq = ab.lengthSq();
		if (abLenSq <= 1e-6f)
		{
			const _float dx = point.x - a.x;
			const _float dz = point.z - a.z;
			return dx * dx + dz * dz;
		}

		const _float t = Engine::clamp(vector3::dot(ap, ab) / abLenSq, 0.f, 1.f);
		const vector3 closest = vector3(a.x + ab.x * t, point.y, a.z + ab.z * t);
		const _float dx = point.x - closest.x;
		const _float dz = point.z - closest.z;
		return dx * dx + dz * dz;
	}

	_float ComputePointToTriangleDistanceSqXZ(const vector<vector3>& vertices, const vector3& point)
	{
		if (vertices.size() < 3)
			return FLT_MAX;

		_float sampleY = 0.f;
		if (TrySampleTriangleHeightAtXZ(vertices, point, sampleY))
			return 0.f;

		_float bestDistSq = FLT_MAX;
		for (_int edge = 0; edge < 3; ++edge)
		{
			const vector3& edgeA = vertices[edge];
			const vector3& edgeB = vertices[(edge + 1) % 3];
			bestDistSq = min(bestDistSq, ComputePointToSegmentDistanceSqXZ(point, edgeA, edgeB));
		}

		for (const vector3& vertex : vertices)
		{
			const _float dx = point.x - vertex.x;
			const _float dz = point.z - vertex.z;
			bestDistSq = min(bestDistSq, dx * dx + dz * dz);
		}

		return bestDistSq;
	}

	vector3 BuildFallbackSamplePoint(const vector<EngineAI::CNaviMesh::NaviPolygon>& polygons, const _uint polygonIndex, const vector3& position)
	{
		const auto& polygon = polygons[polygonIndex];
		vector3 bestPoint = polygon.center;
		_float bestDistSq = FLT_MAX;

		auto testPoint = [&](const vector3& candidate)
		{
			const _float dx = candidate.x - position.x;
			const _float dz = candidate.z - position.z;
			const _float distSq = dx * dx + dz * dz;
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestPoint = candidate;
			}
		};

		testPoint(polygon.center);
		for (const vector3& vertex : polygon.vertices)
			testPoint(vertex);
		for (_int edge = 0; edge < 3; ++edge)
			testPoint(vector3::Lerp(polygon.vertices[edge], polygon.vertices[(edge + 1) % 3], 0.5f));

		return bestPoint;
	}
}

CNaviMesh::CNaviMesh()
	: m_vPolygons({})
{
	m_strName = L"NaviMesh";
}

CNaviMesh::~CNaviMesh()
{
}

HRESULT CNaviMesh::BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes)
{
	UNREFERENCED_PARAMETER(_obstacleMeshes);

	if (!_sourceMesh)
		return E_FAIL;

	vector<VertexTexNormalTangentBuffer> vertices = _sourceMesh->Get_VertexBuffer();
	if (vertices.size() < 3)
		return E_FAIL;

	vector<_uint> indices = _sourceMesh->Get_IndexBuffer();
	vector<vector3> triangles = {};
	triangles.reserve(indices.empty() ? vertices.size() : indices.size());

	auto appendTriangle = [&](const _uint i0, const _uint i1, const _uint i2)
	{
		if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
			return;

		triangles.push_back(vector3(vertices[i0].position));
		triangles.push_back(vector3(vertices[i1].position));
		triangles.push_back(vector3(vertices[i2].position));
	};

	if (!indices.empty())
	{
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
			appendTriangle(indices[i], indices[i + 1], indices[i + 2]);
	}
	else
	{
		for (_uint i = 0; i + 2 < static_cast<_uint>(vertices.size()); i += 3)
			appendTriangle(i, i + 1, i + 2);
	}

	return BuildFromTriangles(triangles);
}

HRESULT CNaviMesh::BuildFromTriangles(const vector<vector3>& triangles)
{
	m_vPolygons.clear();
	if (triangles.size() < 3)
		return E_FAIL;

	for (size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		const vector3& a = triangles[i];
		const vector3& b = triangles[i + 1];
		const vector3& c = triangles[i + 2];
		if (IsDegenerateTriangle(a, b, c))
			continue;

		NaviPolygon polygon = {};
		polygon.index = static_cast<_uint>(m_vPolygons.size());
		polygon.vertices = { a, b, c };
		polygon.center = ComputeTriangleCenter(polygon.vertices);
		m_vPolygons.push_back(move(polygon));
	}

	if (m_vPolygons.empty())
		return E_FAIL;

	constexpr _float edgeTolerance = 0.01f;
	unordered_map<QuantizedEdgeKey, vector<_uint>, QuantizedEdgeKeyHash> edgeOwners = {};
	edgeOwners.reserve(m_vPolygons.size() * 4);

	for (const NaviPolygon& polygon : m_vPolygons)
	{
		for (_int edge = 0; edge < 3; ++edge)
		{
			const vector3& edgeA = polygon.vertices[edge];
			const vector3& edgeB = polygon.vertices[(edge + 1) % 3];
			edgeOwners[MakeEdgeKey(edgeA, edgeB, edgeTolerance)].push_back(polygon.index);
		}
	}

	for (const auto& ownerPair : edgeOwners)
	{
		if (ownerPair.second.size() != 2)
			continue;

		const _uint a = ownerPair.second[0];
		const _uint b = ownerPair.second[1];
		if (a >= m_vPolygons.size() || b >= m_vPolygons.size() || a == b)
			continue;

		auto& neighborsA = m_vPolygons[a].neighbors;
		auto& neighborsB = m_vPolygons[b].neighbors;
		if (find(neighborsA.begin(), neighborsA.end(), b) == neighborsA.end())
			neighborsA.push_back(b);
		if (find(neighborsB.begin(), neighborsB.end(), a) == neighborsB.end())
			neighborsB.push_back(a);
	}

	return S_OK;
}

int CNaviMesh::FindContainingPolygon(const vector3& _position) const
{
	_int bestPolygon = -1;
	_float bestHeightDiff = FLT_MAX;

	for (const NaviPolygon& polygon : m_vPolygons)
	{
		_float sampledY = 0.f;
		if (!TrySampleTriangleHeightAtXZ(polygon.vertices, _position, sampledY))
			continue;

		const _float heightDiff = fabsf(sampledY - _position.y);
		if (heightDiff < bestHeightDiff)
		{
			bestHeightDiff = heightDiff;
			bestPolygon = static_cast<_int>(polygon.index);
		}
	}

	return bestPolygon;
}

bool CNaviMesh::SamplePosition(const vector3& _position, vector3& _outPosition) const
{
	const _int polygonIndex = FindContainingPolygon(_position);
	if (polygonIndex >= 0 && polygonIndex < static_cast<_int>(m_vPolygons.size()))
	{
		_float sampledY = 0.f;
		if (TrySampleTriangleHeightAtXZ(m_vPolygons[polygonIndex].vertices, _position, sampledY))
		{
			_outPosition = vector3(_position.x, sampledY, _position.z);
			return true;
		}
	}

	_uint bestPolygonIndex = 0;
	_float bestDistSq = FLT_MAX;
	for (_uint polygonIndexIter = 0; polygonIndexIter < static_cast<_uint>(m_vPolygons.size()); ++polygonIndexIter)
	{
		const _float distSq = ComputePointToTriangleDistanceSqXZ(m_vPolygons[polygonIndexIter].vertices, _position);
		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			bestPolygonIndex = polygonIndexIter;
		}
	}

	if (bestDistSq == FLT_MAX || m_vPolygons.empty())
		return false;

	_outPosition = BuildFallbackSamplePoint(m_vPolygons, bestPolygonIndex, _position);
	return true;
}

bool CNaviMesh::FindPath(const vector3& _start, const vector3& _end, vector<vector3>& _outPath) const
{
	_outPath.clear();
	if (m_vPolygons.empty())
		return false;

	vector3 sampledStart = {};
	vector3 sampledEnd = {};
	if (!SamplePosition(_start, sampledStart) || !SamplePosition(_end, sampledEnd))
		return false;

	const _int startPolygon = FindContainingPolygon(sampledStart);
	const _int endPolygon = FindContainingPolygon(sampledEnd);
	if (startPolygon < 0 || endPolygon < 0)
		return false;

	if (startPolygon == endPolygon)
	{
		_outPath.push_back(sampledEnd);
		return true;
	}

	const size_t polygonCount = m_vPolygons.size();
	vector<_float> gCosts(polygonCount, FLT_MAX);
	vector<_float> fCosts(polygonCount, FLT_MAX);
	vector<_int> parents(polygonCount, -1);
	vector<_bool> closedSet(polygonCount, false);
	vector<_bool> openSet(polygonCount, false);
	vector<_uint> openList = {};
	openList.reserve(polygonCount);

	auto heuristic = [&](const _uint polygonIndex)
	{
		return (m_vPolygons[polygonIndex].center - sampledEnd).length();
	};

	gCosts[startPolygon] = 0.f;
	fCosts[startPolygon] = heuristic(static_cast<_uint>(startPolygon));
	openList.push_back(static_cast<_uint>(startPolygon));
	openSet[startPolygon] = true;

	while (!openList.empty())
	{
		auto bestIter = openList.begin();
		for (auto iter = openList.begin() + 1; iter != openList.end(); ++iter)
		{
			if (fCosts[*iter] < fCosts[*bestIter])
				bestIter = iter;
		}

		const _uint current = *bestIter;
		openList.erase(bestIter);
		openSet[current] = false;
		closedSet[current] = true;

		if (current == static_cast<_uint>(endPolygon))
			break;

		for (const _uint neighbor : m_vPolygons[current].neighbors)
		{
			if (neighbor >= polygonCount || closedSet[neighbor])
				continue;

			const _float tentativeG = gCosts[current] + (m_vPolygons[current].center - m_vPolygons[neighbor].center).length();
			if (tentativeG >= gCosts[neighbor])
				continue;

			parents[neighbor] = static_cast<_int>(current);
			gCosts[neighbor] = tentativeG;
			fCosts[neighbor] = tentativeG + heuristic(neighbor);
			if (!openSet[neighbor])
			{
				openList.push_back(neighbor);
				openSet[neighbor] = true;
			}
		}
	}

	if (parents[endPolygon] == -1)
		return false;

	vector<_uint> polygonPath = {};
	for (_int polygon = endPolygon; polygon >= 0; polygon = parents[polygon])
	{
		polygonPath.push_back(static_cast<_uint>(polygon));
		if (polygon == startPolygon)
			break;
	}

	if (polygonPath.empty() || polygonPath.back() != static_cast<_uint>(startPolygon))
		return false;

	reverse(polygonPath.begin(), polygonPath.end());

	auto appendWaypoint = [&](const vector3& point)
	{
		if (_outPath.empty() || (_outPath.back() - point).lengthSq() > 0.0001f)
			_outPath.push_back(point);
	};

	appendWaypoint(sampledStart);
	for (size_t i = 1; i + 1 < polygonPath.size(); ++i)
		appendWaypoint(m_vPolygons[polygonPath[i]].center);
	appendWaypoint(sampledEnd);

	if (_outPath.size() >= 3)
	{
		vector<vector3> simplified = {};
		simplified.reserve(_outPath.size());
		simplified.push_back(_outPath.front());
		for (size_t i = 1; i + 1 < _outPath.size(); ++i)
		{
			const vector3 prevDir = (_outPath[i] - simplified.back()).normalized();
			const vector3 nextDir = (_outPath[i + 1] - _outPath[i]).normalized();
			if (prevDir.lengthSq() <= 1e-6f || nextDir.lengthSq() <= 1e-6f)
				continue;
			if (vector3::dot(prevDir, nextDir) < 0.995f)
				simplified.push_back(_outPath[i]);
		}
		simplified.push_back(_outPath.back());
		_outPath = move(simplified);
	}

	if (!_outPath.empty())
		_outPath.erase(_outPath.begin());

	return !_outPath.empty();
}

const vector<CNaviMesh::NaviPolygon>& CNaviMesh::GetPolygons() const
{
	return m_vPolygons;
}

const _bool CNaviMesh::IsEmpty() const
{
	return m_vPolygons.empty();
}

CNaviMesh* CNaviMesh::Create()
{
	return new CNaviMesh();
}