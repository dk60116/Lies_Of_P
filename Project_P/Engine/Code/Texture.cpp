#include "epch.h"
#include "Texture.h"
#include "Resources.h"
#include "DDSTextureLoader.h"

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
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	if (!device)
		return E_FAIL;

	wstring extension = filesystem::path(m_strFilePath).extension().wstring();
	transform(extension.begin(), extension.end(), extension.begin(), towlower);

	HRESULT hr = E_FAIL;
	if (extension == L".dds")
	{
		hr = DirectX::CreateDDSTextureFromFile(device, context, m_strFilePath.c_str(), nullptr, &m_pSRV);
		if (FAILED(hr))
		{
			const HRESULT rebuildResult = CResources::GetInstance().RebuildDDSFromBinaryPath(m_strFilePath);
			if (SUCCEEDED(rebuildResult))
			{
				Safe_Release(m_pSRV);
				hr = DirectX::CreateDDSTextureFromFile(device, context, m_strFilePath.c_str(), nullptr, &m_pSRV);
				if (SUCCEEDED(hr))
					CDebug::LogWarnning(L"Texture load recovered by rebuilding DDS: " + m_strFilePath);
			}
		}
	}
	else
		hr = CreateWICTextureFromFile(device, context, m_strFilePath.c_str(), nullptr, &m_pSRV);

	if (FAILED(hr))
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
