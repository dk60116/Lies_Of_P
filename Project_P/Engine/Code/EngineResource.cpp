#include "epch.h"
#include "EngineResource.h"

CEngineResource::CEngineResource()
	: m_strResourceName({})
	, m_strFilePath({})
{
}

CEngineResource::~CEngineResource()
{
	OnDestroy();
}

HRESULT CEngineResource::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	m_strResourceName = _name;
	m_strFilePath = _filePath;

	return S_OK;
}

const wstring& CEngineResource::Get_ResourceName() const
{
	return m_strResourceName;
}

void CEngineResource::Set_ResourceName(const wstring& _name)
{
	m_strResourceName = _name;
}

void CEngineResource::OnDestroy()
{
}
