#include "epch.h"
#include "RenderTargetManager.h"

CRenderTargetManager::CRenderTargetManager()
    : m_width(0)
    , m_height(0)
    , m_rtList({})
{
}

CRenderTargetManager::~CRenderTargetManager()
{
}

CRenderTargetManager& CRenderTargetManager::GetInstance()
{
    static CRenderTargetManager inst;
    return inst;
}

HRESULT CRenderTargetManager::Initialize()
{
    ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

    const int width = CDisplay::GetInstance().Get_ScreenResolution().x;
    const int height = CDisplay::GetInstance().Get_ScreenResolution().y;

    if (!device || width == 0 || height == 0)
        return E_FAIL;

    Destroy();
    return CreateTargets(device, width, height);
}

void CRenderTargetManager::Destroy()
{
    for (auto& kv : m_rtList)
        kv.second.Destroy();

    m_rtList.clear();
    m_width = 0;
    m_height = 0;
}

HRESULT CRenderTargetManager::Resize(ID3D11Device* device, UINT width, UINT height)
{
    if (!device || width == 0 || height == 0)
        return E_FAIL;

    if (width == m_width && height == m_height)
        return S_OK;

    Destroy();
    return CreateTargets(device, static_cast<_uint>(width), static_cast<_uint>(height));
}

void CRenderTargetManager::Bind_RenderTarget(const CRenderTarget::RTType type, ID3D11DeviceContext* context, const D3D11_VIEWPORT* vp)
{
    if (!context)
        return;

    auto it = m_rtList.find(type);
    if (it == m_rtList.end())
        return;

    Unbind_AllSRVs_PS(context);

    CRenderTarget& rt = it->second;

    if (type == CRenderTarget::RTType::Depth || type == CRenderTarget::RTType::ShadowDepth)
    {
        ID3D11DepthStencilView* dsv = rt.GetDSV();
        if (!dsv)
            return;

        context->OMSetRenderTargets(0, nullptr, dsv);

        if (vp)
            context->RSSetViewports(1, vp);

        return;
    }

    ID3D11RenderTargetView* rtv = rt.GetRTV();
    if (!rtv)
        return;

    ID3D11DepthStencilView* dsv = nullptr;
    auto itDepth = m_rtList.find(CRenderTarget::RTType::Depth);
    if (itDepth != m_rtList.end())
        dsv = itDepth->second.GetDSV();

    context->OMSetRenderTargets(1, &rtv, dsv);

    if (vp)
        context->RSSetViewports(1, vp);
}

void CRenderTargetManager::Bind_GBuffer(ID3D11DeviceContext* ctx, const D3D11_VIEWPORT* vp)
{
    Unbind_AllSRVs_PS(ctx);

    auto rtvA = GetRTV(CRenderTarget::RTType::Albedo);
    auto rtvN = GetRTV(CRenderTarget::RTType::Normal);
    auto rtvM = GetRTV(CRenderTarget::RTType::Material);
    auto dsv = GetDSV(CRenderTarget::RTType::Depth);

    if (!rtvA || !rtvN || !rtvM || !dsv)
        return;

    ID3D11RenderTargetView* rtvs[3] = { rtvA, rtvN, rtvM };
    ctx->OMSetRenderTargets(3, rtvs, dsv);

    if (vp) 
        ctx->RSSetViewports(1, vp);
}

void CRenderTargetManager::Clear_RenderTarget(const CRenderTarget::RTType type)
{
    ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

    if (!context)
        return;

    auto it = m_rtList.find(type);

    if (it == m_rtList.end())
        return;

    CRenderTarget& rt = it->second;

    if (type == CRenderTarget::RTType::Depth || type == CRenderTarget::RTType::ShadowDepth)
    {
        ID3D11DepthStencilView* dsv = rt.GetDSV();
        if (!dsv)
            return;

        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        return;
    }

    _float clear[4] = { 0, 0, 0, 0 };

    if (type == CRenderTarget::RTType::Normal)
    {
        clear[0] = 0.5f;
        clear[1] = 0.5f;
        clear[2] = 1.0f;
        clear[3] = 1.0f;
    }
    else if (type == CRenderTarget::RTType::ShadowMask)
    {
        clear[0] = 1.0f;
        clear[1] = 1.0f;
        clear[2] = 1.0f;
        clear[3] = 1.0f;
    }

    ID3D11RenderTargetView* rtv = rt.GetRTV();

    if (!rtv)
        return;

    context->ClearRenderTargetView(rtv, clear);
}

void CRenderTargetManager::Clear_GBuffer()
{
    Clear_RenderTarget(CRenderTarget::RTType::Albedo);
    Clear_RenderTarget(CRenderTarget::RTType::Normal);
    Clear_RenderTarget(CRenderTarget::RTType::Material);
    Clear_RenderTarget(CRenderTarget::RTType::Depth);
    Clear_RenderTarget(CRenderTarget::RTType::Specular);
    Clear_RenderTarget(CRenderTarget::RTType::ShadowMask);
    Clear_RenderTarget(CRenderTarget::RTType::Combine);
}

ID3D11RenderTargetView* CRenderTargetManager::GetRTV(const CRenderTarget::RTType type) const
{
    auto it = m_rtList.find(type);

    if (it == m_rtList.end())
        return nullptr;

    return it->second.GetRTV();
}

ID3D11ShaderResourceView* CRenderTargetManager::GetSRV(const CRenderTarget::RTType type) const
{
    auto it = m_rtList.find(type);

    if (it == m_rtList.end())
        return nullptr;

    return it->second.GetSRV();
}

ID3D11DepthStencilView* CRenderTargetManager::GetDSV(const CRenderTarget::RTType type) const
{
    auto it = m_rtList.find(type);

    if (it == m_rtList.end())
        return nullptr;

    return it->second.GetDSV();
}

void CRenderTargetManager::Unbind_AllSRVs_PS(ID3D11DeviceContext* context)
{
    if (!context)
        return;

    ID3D11ShaderResourceView* nullSRV[16] = {};
    context->PSSetShaderResources(0, 16, nullSRV);
}

HRESULT CRenderTargetManager::CreateTargets(ID3D11Device* device, _uint width, _uint height)
{
    m_width = width;
    m_height = height;

    if (FAILED(m_rtList[CRenderTarget::RTType::Combine].Create(CRenderTarget::RTType::Combine, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Albedo] .Create(CRenderTarget::RTType::Albedo, device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Normal].Create(CRenderTarget::RTType::Normal, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Material].Create(CRenderTarget::RTType::Material, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Depth].Create(CRenderTarget::RTType::Depth, device, width, height, DXGI_FORMAT_R24G8_TYPELESS, true)))
        return E_FAIL;

    _uint shadowMapSize = CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;

    if (FAILED(m_rtList[CRenderTarget::RTType::ShadowDepth].Create(CRenderTarget::RTType::ShadowDepth, device, shadowMapSize, shadowMapSize, DXGI_FORMAT_R32_TYPELESS, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Shading].Create(CRenderTarget::RTType::Shading, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::Specular].Create(CRenderTarget::RTType::Specular, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(m_rtList[CRenderTarget::RTType::ShadowMask].Create(CRenderTarget::RTType::ShadowMask, device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true)))
        return E_FAIL;

    return S_OK;
}
