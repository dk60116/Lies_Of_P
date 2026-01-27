#pragma once

#include "MeshBuffer.h"

NS_BEGIN(Engine)

NS_BEGIN(EngineAI)

class ENGINE_DLL CNaviMesh final : public CMeshBuffer
{
	friend class CResources;

	struct NavBakeOptions
	{
		
	};

public:
	struct NaviPolygon
	{
		_uint index;
		vector<_uint> neighbors;
		vector<vector3> vertices;
	};

	struct PathNode
	{
		_uint polygonIndex;
		float gCost, hCost;
		PathNode* parent;
	};

private:
	CNaviMesh();
	~CNaviMesh();

public:
	HRESULT BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes);
	bool FindPath(const vector3& _start, const vector3& _end, vector<vector3>& _outPath);
	int FindContainingPolygon(const vector3& _position);

private:
	static CNaviMesh* Create();
};

NS_END

NS_END

