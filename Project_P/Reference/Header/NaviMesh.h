#pragma once

#include "MeshBuffer.h"

NS_BEGIN(Engine)
NS_BEGIN(EngineAI)

class ENGINE_DLL CNaviMesh final : public CMeshBuffer
{
public:
	struct NaviPolygon
	{
		_uint index = 0;
		vector<_uint> neighbors = {};
		vector<vector3> vertices = {};
		vector3 center = {};
	};

protected:
	CNaviMesh();
	~CNaviMesh();

public:
	HRESULT BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes);
	HRESULT BuildFromTriangles(const vector<vector3>& triangles);
	bool FindPath(const vector3& _start, const vector3& _end, vector<vector3>& _outPath) const;
	int FindContainingPolygon(const vector3& _position) const;
	bool SamplePosition(const vector3& _position, vector3& _outPosition) const;
	const vector<NaviPolygon>& GetPolygons() const;
	const _bool IsEmpty() const;

public:
	static CNaviMesh* Create();

private:
	vector<NaviPolygon> m_vPolygons;
};

NS_END
NS_END