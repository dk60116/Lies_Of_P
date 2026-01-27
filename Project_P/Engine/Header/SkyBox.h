#pragma once

#include "MeshBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSkyBox final : public CMeshBuffer
{
	friend class CResources;

public:
	typedef struct TerainMeshBufferDesctiption
	{
		wstring texture;
	}SKYBOXBUFFERDESC;

protected:
	explicit CSkyBox();
	~CSkyBox();

protected:
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;

public:
	HRESULT Initialize_Scene();

private:
	static CSkyBox* Create();
	void OnDestroy();

public:
	void RenderSky(CCamera* _camera);

	SKYBOXBUFFERDESC m_sSaveDesc;
	CMaterial* m_pMaterial;
};

NS_END

