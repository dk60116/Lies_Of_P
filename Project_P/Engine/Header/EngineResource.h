#pragma once

#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL CEngineResource abstract : public UObject
{
	friend class CResources;

protected:
		explicit CEngineResource();
		~CEngineResource();

protected:
	virtual HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc);
	virtual void OnDestroy();

public:
	const wstring& Get_ResourceName() const;
	const wstring& Get_FilePath() const;
	void Set_ResourceName(const wstring& _name);
	void Set_FilePath(const wstring& _path);

protected:
	_uint m_resourceID;
	wstring m_strResourceName;
	wstring m_strFilePath;
};

NS_END

