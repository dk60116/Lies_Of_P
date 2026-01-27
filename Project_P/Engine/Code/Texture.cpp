#include "epch.h"
#include "Texture.h"

CTexture::CTexture()
	: m_pTexture(nullptr)
	, m_pSRV(nullptr)
	, m_sTextureDesc({})
{
	m_strName = L"Texture";
}

CTexture::~CTexture()
{
	OnDestroy();
}

CTexture* CTexture::Create()
{
	return new CTexture();
}

void CTexture::OnDestroy()
{
	Safe_Release(m_pTexture);
	Safe_Release(m_pSRV);
}

HRESULT CTexture::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	if (_filePath.size() <= 0)
		return S_OK;

	if (FAILED(__super::Initialize(_name, _filePath, _desc)))
		return E_FAIL;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return E_FAIL;

	if (FAILED(CreateWICTextureFromFile(device, m_strFilePath.c_str(), nullptr, &m_pSRV)))
	{
		CDebug::LogError(L"Texture load failed - Can not create SRV: " + m_strFilePath);
		return E_FAIL;
	}

	m_pSRV->GetResource(reinterpret_cast<ID3D11Resource**>(&m_pTexture));

	if (!m_pTexture)
	{
		CDebug::LogError(L"Texture load failed - Can not create Texture: " + m_strFilePath);
		return E_FAIL;
	}

	m_pTexture->GetDesc(&m_sTextureDesc);

	return S_OK;
}

ID3D11Texture2D* CTexture::Get_Texture() const
{
	return m_pTexture;
}

ID3D11ShaderResourceView* CTexture::Get_SRV() const
{
	return m_pSRV;
}

const D3D11_TEXTURE2D_DESC& CTexture::Get_TextureDesc()
{
	return m_sTextureDesc;
}
