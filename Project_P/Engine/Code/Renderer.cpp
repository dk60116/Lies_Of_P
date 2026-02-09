#include "epch.h"
#include "Renderer.h"

CRenderer::CRenderer()
	: m_pMaterial(nullptr)
	, m_pOutlineMat(nullptr)
	, m_bCastShadow(true)
	, m_fSclaeFactor(1.f)
	, m_bUseInstancing(false)
	, m_iInstanceCount(0)
	, m_pInstanceBuffer(nullptr)
	, m_vInstanceTransforms({})
{
}

CRenderer::~CRenderer()
{
}

void CRenderer::Update()
{
	if (m_pMaterial)
	{
		_uint id = m_pGameObject->Get_UniqueID();
		m_pMaterial->Set_IntValue(L"gObjectID", id);
	}
}

void CRenderer::OnDestroy()
{
	Safe_Release(m_pMaterial);
	Safe_Release(m_pOutlineMat);
	Safe_Release(m_pInstanceBuffer);
}

HRESULT CRenderer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	if (!m_pInstanceBuffer)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(InstanceCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pInstanceBuffer)))
			return E_FAIL;
	}

	if (!m_pMaterial)
	{
		Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferLit (Material)"));
	}

	if (!m_pOutlineMat)
	{

	}
	
	if (m_pOutlineMat)
		m_pOutlineMat->AddRef();

	CShader* outShader = CResources::GetInstance().LoadOnGame<CShader>(L"Outline (Shader)");

	if (!outShader)
	{
		CDebug::LogError("Not found outline shader");
		return E_FAIL;
	}

	//m_pOutlineMat->Set_Shader(outShader);

	return S_OK;
}

CMaterial* CRenderer::Get_Material()
{
	return m_pMaterial;
}

void CRenderer::Set_Material(CMaterial* _material)
{
	Safe_Release(m_pMaterial);

	m_pMaterial = _material;

	if (m_pMaterial)
		m_pMaterial->AddRef();
}

const _bool CRenderer::IsCastShadow() const
{
	return m_bCastShadow;
}

void CRenderer::SetCastShadow(const _bool _on)
{
	m_bCastShadow = _on;
}

void CRenderer::CreateMeshInstancing(const _uint _count)
{
	if (_count == 0)
	{
		m_bUseInstancing = false;
		m_iInstanceCount = 0;
		m_vInstanceTransforms.clear();
		return;
	}

	const _uint maxCount = 128;
	m_iInstanceCount = min(_count, maxCount);
	m_bUseInstancing = true;

	m_vInstanceTransforms.assign(m_iInstanceCount, InstanceTransform{});

	Safe_Release(m_pInstanceBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(InstanceCB);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pInstanceBuffer)))
	{
		m_bUseInstancing = false;
		m_iInstanceCount = 0;
	}
}

void CRenderer::SetInstancingPosition(const _uint _index, const vector3& _pos)
{
	if (!m_bUseInstancing || _index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].position = _pos;
}

void CRenderer::SetInstancingRotation(const _uint _index, const vector3& _rot)
{
	if (!m_bUseInstancing || _index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].rotation = _rot;
}

void CRenderer::SetInstancingSize(const _uint _index, const vector3& _size)
{
	if (!m_bUseInstancing || _index >= m_vInstanceTransforms.size())
		return;

	m_vInstanceTransforms[_index].scale = _size;
}

const _bool CRenderer::IsInstancingEnabled() const
{
	return m_bUseInstancing && m_iInstanceCount > 0 && m_pInstanceBuffer;
}

const _uint CRenderer::GetInstanceCount() const
{
	return m_iInstanceCount;
}

void CRenderer::Bind_InstanceBuffer(const _matrix& _baseWorld)
{
	if (!m_pInstanceBuffer)
		return;

	InstanceCB cb = {};

	size_t count = 0;
	if (IsInstancingEnabled())
	{
		count = min<size_t>(m_iInstanceCount, 128);

		for (size_t i = 0; i < count; ++i)
		{
			const auto& tr = m_vInstanceTransforms[i];
			_matrix scaleMat = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z);
			_vector rotVec = XMVectorSet(
				XMConvertToRadians(tr.rotation.x),
				XMConvertToRadians(tr.rotation.y),
				XMConvertToRadians(tr.rotation.z),
				0.f
			);
			_matrix rotMat = XMMatrixRotationRollPitchYawFromVector(rotVec);
			_matrix transMat = XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);
			_matrix worldMat = _baseWorld * scaleMat * rotMat * transMat;
			cb.worlds[i] = XMMatrixTranspose(worldMat);
		}
	}

	cb.instanceCount = static_cast<_uint>(count);

	m_pContext->UpdateSubresource(m_pInstanceBuffer, 0, nullptr, &cb, 0, 0);
	m_pContext->VSSetConstantBuffers(4, 1, &m_pInstanceBuffer);
}
