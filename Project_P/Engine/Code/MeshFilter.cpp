#include "epch.h"
#include "MeshFilter.h"
#include "Transform.h"

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

const _float CMeshFilter::GetScaleFactor() const
{
	if (!m_pGameObject || !m_pGameObject->GetTransform())
		return 1.f;

	return m_pGameObject->GetTransform()->Get_LocalScale().x;
}

void CMeshFilter::SetScaleFactor(const _float _value)
{
	if (!m_pGameObject || !m_pGameObject->GetTransform())
		return;

	const _float clampedValue = max(_value, 0.0001f);
	m_pGameObject->GetTransform()->Set_LocalScale(vector3::one() * clampedValue);
}

vector3 CMeshFilter::GetRotationFactor() const
{
	if (!m_pGameObject || !m_pGameObject->GetTransform())
		return vector3::zero();

	return m_pGameObject->GetTransform()->Get_LocalEulerAngles();
}

void CMeshFilter::SetRotationFactor(const vector3& _value)
{
	if (!m_pGameObject || !m_pGameObject->GetTransform())
		return;

	m_pGameObject->GetTransform()->Set_LocalEulerAngles(_value);
}
