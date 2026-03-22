#include "epch.h"
#include "Image.h"

namespace
{
	_int GetMaxFillOrigin(CImage::FillMethod fillMethod)
	{
		switch (fillMethod)
		{
		case CImage::FillMethod::Horizontal:
		case CImage::FillMethod::Vertical:
			return 1;
		case CImage::FillMethod::Radial90:
		case CImage::FillMethod::Radial180:
		case CImage::FillMethod::Radial360:
			return 3;
		case CImage::FillMethod::None:
		default:
			return 0;
		}
	}
}

CImage::CImage()
	: m_pTexture(nullptr)
	, m_pImageBuffer(nullptr)
	, m_eFillMethod(FillMethod::None)
	, m_fFillAmount(1.f)
	, m_iFillOrigin(0)
	, m_bFillClockwise(true)
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
	clone->m_iFillOrigin = this->m_iFillOrigin;
	clone->m_bFillClockwise = this->m_bFillClockwise;

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
	m_iFillOrigin = clamp(m_iFillOrigin, 0, GetMaxFillOrigin(m_eFillMethod));
}

_int CImage::Get_FillOrigin() const
{
	return m_iFillOrigin;
}

void CImage::Set_FillOrigin(_int _fillOrigin)
{
	m_iFillOrigin = clamp(_fillOrigin, 0, GetMaxFillOrigin(m_eFillMethod));
}

_bool CImage::Get_FillClockwise() const
{
	return m_bFillClockwise;
}

void CImage::Set_FillClockwise(_bool _fillClockwise)
{
	m_bFillClockwise = _fillClockwise;
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
	ImageCB imageValue =
	{
		_float4
		(
			m_fFillAmount,
			static_cast<_float>(m_eFillMethod),
			static_cast<_float>(m_iFillOrigin),
			m_bFillClockwise ? 1.f : 0.f
		)
	};
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

CTexture* CImage::GetTexture() const
{
	return m_pTexture;
}
