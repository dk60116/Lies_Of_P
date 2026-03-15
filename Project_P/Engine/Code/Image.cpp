#include "epch.h"
#include "Image.h"

CImage::CImage()
	: m_pTexture(nullptr)
	, m_pImageBuffer(nullptr)
	, m_eFillMethod(FillMethod::None)
	, m_fFillAmount(1.f)
{
	m_strName = L"Image";
}

CImage::~CImage()
{
}

CImage* CImage::Create()
{
	return new CImage();
}

CComponent* CImage::Clone() const
{
	CImage* clone = new CImage;

	clone->m_eFillMethod = this->m_eFillMethod;
	clone->m_fFillAmount = this->m_fFillAmount;

	if (this->m_pTexture)
		clone->SetTexture(this->m_pTexture);

	return clone;
}

HRESULT CImage::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(ImageCB);

	if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pImageBuffer)))
		return E_FAIL;

	return S_OK;
}

void CImage::Render_Editor()
{
}

void CImage::Render()
{
}

void CImage::OnDestroy()
{
	__super::OnDestroy();

	Safe_Release(m_pTexture);
}

const CImage::FillMethod CImage::Get_FillMethod() const
{
	return m_eFillMethod;
}

void CImage::Set_FillMethod(FillMethod _fillMethod)
{
	m_eFillMethod = _fillMethod;
}

const _float CImage::GetFillAmount() const
{
	return m_fFillAmount;
}

void CImage::SetFillAmount(_float _fill)
{
	_fill = clamp(_fill, 0.f, 1.f);

	m_fFillAmount = _fill;
}

void CImage::Bind_UIMaterial()
{
	ImageCB imageValue = { _float4(m_fFillAmount, 0, 0, 0) };
	m_pContext->UpdateSubresource(m_pImageBuffer, 0, nullptr, &imageValue, 0, 0);
	m_pContext->PSSetConstantBuffers(3, 1, &m_pImageBuffer);
}

void CImage::SetTexture(CTexture* _texture)
{
	Safe_Release(m_pTexture);

	m_pTexture = _texture;

	if (m_pTexture)
	{
		m_pTexture->AddRef();

		m_pMaterial->Set_Texture(m_pTexture, 0);
	}
}
