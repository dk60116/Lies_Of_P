#include "epch.h"
#include "RenderTarget.h"

CRenderTarget::CRenderTarget()
    : m_type(RTType::Albedo)
    , m_width(0)
    , m_height(0)
    , m_format(DXGI_FORMAT_UNKNOWN)
    , m_createSRV(true)
    , m_texture(nullptr)
    , m_rtv(nullptr)
    , m_dsv(nullptr)
    , m_srv(nullptr)
{
}

CRenderTarget::~CRenderTarget()
{
    Destroy();
}

void CRenderTarget::Destroy()
{
    Safe_Release(m_srv);
    Safe_Release(m_dsv);
    Safe_Release(m_rtv);
    Safe_Release(m_texture);

    m_type = RTType::Albedo;
    m_width = 0;
    m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
    m_createSRV = true;
}

CRenderTarget::RTType CRenderTarget::GetType() const
{
    return m_type;
}

vector2Int CRenderTarget::GetWidthHeight() const
{
    return vector2Int(m_width, m_height);
}

DXGI_FORMAT CRenderTarget::GetFormat() const
{
    return m_format;
}

bool CRenderTarget::IsCreateSRV() const
{
    return m_createSRV;
}

ID3D11Texture2D* CRenderTarget::GetTexture() const
{
    return m_texture;
}

ID3D11RenderTargetView* CRenderTarget::GetRTV() const
{
    return m_rtv;
}

ID3D11DepthStencilView* CRenderTarget::GetDSV() const
{
    return m_dsv;
}

ID3D11ShaderResourceView* CRenderTarget::GetSRV() const
{
    return m_srv;
}

const bool CRenderTarget::IsDepth() const
{
    return m_type == RTType::Depth;
}

const bool CRenderTarget::HasRTV() const
{
    return m_rtv != nullptr;
}

const bool CRenderTarget::HasDSV() const
{
    return m_dsv != nullptr;
}

const bool CRenderTarget::HasSRV() const
{
    return m_srv != nullptr;
}

HRESULT CRenderTarget::Create(RTType type, ID3D11Device* device, _uint width, _uint height, DXGI_FORMAT format, bool createSRV)
{
    if (!device || width == 0 || height == 0)
        return E_FAIL;

    Destroy();

    m_type = type;
    m_width = width;
    m_height = height;
    m_format = format;
    m_createSRV = createSRV;

    if (type == RTType::Depth)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc.Count = 1;
        td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | (createSRV ? D3D11_BIND_SHADER_RESOURCE : 0);
        td.CPUAccessFlags = 0;
        td.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
        if (FAILED(hr)) 
            return hr;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
        dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvd.Texture2D.MipSlice = 0;

        hr = device->CreateDepthStencilView(m_texture, &dsvd, &m_dsv);
        if (FAILED(hr))
            return hr;

        if (createSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MostDetailedMip = 0;
            srvd.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(m_texture, &srvd, &m_srv);
            if (FAILED(hr)) 
                return hr;
        }

        return S_OK;
    }

    if (type == RTType::ShadowDepth)
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | (createSRV ? D3D11_BIND_SHADER_RESOURCE : 0);
        td.CPUAccessFlags = 0;
        td.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
        if (FAILED(hr))
            return hr;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvd.Texture2D.MipSlice = 0;

        hr = device->CreateDepthStencilView(m_texture, &dsvd, &m_dsv);
        if (FAILED(hr)) 
            return hr;

        if (createSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format = DXGI_FORMAT_R32_FLOAT;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MostDetailedMip = 0;
            srvd.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(m_texture, &srvd, &m_srv);
            if (FAILED(hr)) return hr;
        }

        return S_OK;
    }

    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = format;
        td.SampleDesc.Count = 1;
        td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | (createSRV ? D3D11_BIND_SHADER_RESOURCE : 0);
        td.CPUAccessFlags = 0;
        td.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
        if (FAILED(hr)) 
            return hr;

        hr = device->CreateRenderTargetView(m_texture, nullptr, &m_rtv);
        if (FAILED(hr)) 
            return hr;

        if (createSRV)
        {
            hr = device->CreateShaderResourceView(m_texture, nullptr, &m_srv);
            if (FAILED(hr)) 
                return hr;
        }

        return S_OK;
    }

    return E_FAIL;
}

HRESULT CRenderTarget::CreateColor_Internal(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = m_format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (m_createSRV)
        texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr))
        return E_FAIL;

    hr = device->CreateRenderTargetView(m_texture, nullptr, &m_rtv);
    if (FAILED(hr))
        return E_FAIL;

    if (m_createSRV)
    {
        hr = device->CreateShaderResourceView(m_texture, nullptr, &m_srv);
        if (FAILED(hr))
            return E_FAIL;
    }

    Safe_Release(m_dsv);

    return S_OK;
}

HRESULT CRenderTarget::CreateDepth_Internal(ID3D11Device* device)
{
    DXGI_FORMAT typelessFmt = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dsvFmt = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT srvFmt = DXGI_FORMAT_UNKNOWN;

    if (m_createSRV)
    {
        if (!GetDepthTypelessFormats(m_format, typelessFmt, dsvFmt, srvFmt))
            return E_FAIL;
    }
    else
    {
        typelessFmt = m_format;
        dsvFmt = m_format;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = typelessFmt;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (m_createSRV)
        texDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr))
        return E_FAIL;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = dsvFmt;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(m_texture, &dsvDesc, &m_dsv);
    if (FAILED(hr))
        return E_FAIL;

    if (m_createSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = srvFmt;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(m_texture, &srvDesc, &m_srv);
        if (FAILED(hr))
            return E_FAIL;
    }

    Safe_Release(m_rtv);

    return S_OK;
}

bool CRenderTarget::GetDepthTypelessFormats(
    DXGI_FORMAT dsvFormat,
    DXGI_FORMAT& outTypelessTexFormat,
    DXGI_FORMAT& outDsvFormat,
    DXGI_FORMAT& outSrvFormat)
{
    switch (dsvFormat)
    {
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        outTypelessTexFormat = DXGI_FORMAT_R24G8_TYPELESS;
        outDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        outSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        return true;

    case DXGI_FORMAT_D32_FLOAT:
        outTypelessTexFormat = DXGI_FORMAT_R32_TYPELESS;
        outDsvFormat = DXGI_FORMAT_D32_FLOAT;
        outSrvFormat = DXGI_FORMAT_R32_FLOAT;
        return true;

    default:
        return false;
    }
}