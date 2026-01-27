#include "epch.h"
#include "MeshFilter.h"

CMeshFilter::CMeshFilter()
	: m_pMeshBuffer(nullptr)
{
	m_strName = L"Mesh Filter";
}

CMeshFilter::~CMeshFilter()
{
}

void CMeshFilter::OnDestroy()
{
	Safe_Release(m_pMeshBuffer);
}

CMeshFilter* CMeshFilter::Create()
{
	return new CMeshFilter();
}

CComponent* CMeshFilter::Clone() const
{
	CMeshFilter* clone = new CMeshFilter();

	clone->Set_MeshBuffer(this->m_pMeshBuffer);

	return clone;
}

HRESULT CMeshFilter::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CMeshFilter::Set_MeshBuffer(CMeshBuffer* _buffer)
{
	if (m_pMeshBuffer == _buffer)
		return;

	if (m_pMeshBuffer)
		Safe_Release(m_pMeshBuffer);

	m_pMeshBuffer = _buffer;

	if (m_pMeshBuffer)
		m_pMeshBuffer->AddRef();
}

CMeshBuffer* CMeshFilter::Get_MeshBuffer() const
{
	return m_pMeshBuffer;
}
