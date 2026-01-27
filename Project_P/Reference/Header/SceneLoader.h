#pragma once

#include "epch.h"
#include "MeshBuffer.h"
#include "SkyBox.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSceneLoader final
{
	SINGLETONCLASS(CSceneLoader);

private:
	static unsigned __stdcall ThreadMain(void* pParam);

public:
	HRESULT Initialize();

public:
	const _bool Is_Loading() const;
	const _float Get_LoadingProgress() const;

public:
	void StartLoading(vector<string>& _nameList, vector<string>& _fileList, vector<string>& _formatList);

private:
	void ThreadLoadingLoop();
	void Shutdown();

private:
	CSkyBox::SKYBOXBUFFERDESC FormatToSkyBoxDesc(const wstring& _name, const wstring& _format) const;
	CMeshBuffer::TERRAINBUFFERDESC FormatToTerrainDesc(const wstring& _name, const wstring& _format) const;

private:
	HANDLE m_hThread;
	CRITICAL_SECTION m_pCriticalSection;

	vector<string> m_mReadyFiles_Name, m_mReadyFiles_Path, m_mReadyFiles_Format;

	_bool m_bRunning;
	_bool m_bLoading;

	_uint m_iTootalFile;
	_uint m_iLoadedFile;
};

NS_END
