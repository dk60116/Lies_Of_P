#include "epch.h"
#include "Object.h"
#include "Scene.h"

UObject::UObject()
	: m_iInstanceId(0)
	, m_iRefCount(0)
{
}

UObject::~UObject()
{
}

void UObject::AddRef()
{
	++m_iRefCount;
}

_uint UObject::Release()
{
    _uint prevRefCount = m_iRefCount;
    _uint refCount = --m_iRefCount;

    if (refCount == 0)
        delete this;

    return prevRefCount;
}

const wstring& UObject::Get_UName() const
{
    return m_strName;
}

const UINT UObject::Get_RefCnt() const
{
    return m_iRefCount;
}
