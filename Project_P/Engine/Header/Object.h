#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL UObject abstract
{
public:
	UObject();
	virtual ~UObject();

public:
	void AddRef();
	_uint Release();

public:
	const wstring& Get_UName() const;
	const _uint Get_RefCnt() const;

protected:
	wstring m_strName;
	_uint m_iInstanceId;

private:
	static atomic<_uint> s_iNextInstanceID;
	atomic<_uint> m_iRefCount;
};

NS_END