#include "epch.h"
#include "NaviMesh.h"

using namespace EngineAI;

CNaviMesh::CNaviMesh()
{
}

CNaviMesh::~CNaviMesh()
{
}

HRESULT CNaviMesh::BuildFromMesh(CMeshBuffer* _sourceMesh, vector<CMeshBuffer*> _obstacleMeshes)
{
	return S_OK;
}

CNaviMesh* CNaviMesh::Create()
{
	return new CNaviMesh();
}