#include "epch.h"
#include "RenderTargetManager.h"

CRenderTargetManager::CRenderTargetManager()
    : m_iWidth(0)
    , m_iHeight(0)
    , m_iWidth_E(0)
    , m_iHeight_E(0)
    , m_rtList({})
    , m_rtList_E({})
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

    const _int width = CDisplay::GetInstance().Get_ScreenResolution().x;
    const _int height = CDisplay::GetInstance().Get_ScreenResolution().y;

    const _int width_E = CEditor::GetInstance().Get_ScreenResolution().x;
    const _int height_E = CEditor::GetInstance().Get_ScreenResolution().y;

    if (!device || width <= 0 || height <= 0)
        return E_FAIL;

    if (FAILED(CreateTargets(device, (_uint)width, (_uint)height)))
        return E_FAIL;
    if (FAILED(CreateTargets(device, (_uint)width_E, (_uint)height_E, true)))
        return E_FAIL;

    return S_OK;
}

void CRenderTargetManager::Release()
{
    for (auto& kv : m_rtList)
        kv.second.Destroy();

    m_rtList.clear();
    m_iWidth = 0;
    m_iHeight = 0;

    for (auto& kv : m_rtList_E)
        kv.second.Destroy();

    m_rtList_E.clear();
    m_iWidth_E = 0;
    m_iHeight_E = 0;
}

HRESULT CRenderTargetManager::Resize(ID3D11Device* device, UINT width, UINT height, const _bool _isEditor)
{
    if (!device || width == 0 || height == 0)
        return E_FAIL;

    _uint& crtW = _isEditor ? m_iWidth_E : m_iWidth;
    _uint& crtH = _isEditor ? m_iHeight_E : m_iHeight;

    if (crtW == width && crtH == height)
        return S_OK;

    auto& rtMap = PickRTMap(this, _isEditor);
    for (auto& kv : rtMap)
        kv.second.Destroy();
    rtMap.clear();

    crtW = 0;
    crtH = 0;

    return CreateTargets(device, (_uint)width, (_uint)height, _isEditor);
}

void CRenderTargetManager::Bind_RenderTarget(const CRenderTarget::RTType type, ID3D11DeviceContext* context, const D3D11_VIEWPORT* vp, const _bool _isEditor)
{
    if (!context)
        return;

    auto& rtMap = PickRTMap(this, _isEditor);
    auto it = rtMap.find(type);
    if (it == rtMap.end())
        return;

    Unbind_AllSRVs_PS(context); // SRV/RTV 해저드 방지 (필수)

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
    auto itDepth = rtMap.find(CRenderTarget::RTType::Depth);
    if (itDepth != rtMap.end())
        dsv = itDepth->second.GetDSV();

    context->OMSetRenderTargets(1, &rtv, dsv);
    if (vp) 
        context->RSSetViewports(1, vp);
}

void CRenderTargetManager::Bind_GBuffer(ID3D11DeviceContext* ctx, const D3D11_VIEWPORT* vp, const _bool _isEditor)
{
    if (!ctx)
        return;

    Unbind_AllSRVs_PS(ctx);

    ID3D11RenderTargetView* rtvA = GetRTV(CRenderTarget::RTType::Albedo, _isEditor);
    ID3D11RenderTargetView* rtvO = GetRTV(CRenderTarget::RTType::Object, _isEditor);
    ID3D11RenderTargetView* rtvN = GetRTV(CRenderTarget::RTType::Normal, _isEditor);
    ID3D11RenderTargetView* rtvM = GetRTV(CRenderTarget::RTType::Material, _isEditor);
    ID3D11DepthStencilView* dsv = GetDSV(CRenderTarget::RTType::Depth, _isEditor);

    if (!rtvA || !rtvO || !rtvN || !rtvM || !dsv)
        return;

    ID3D11RenderTargetView* rtvs[4] = { rtvA, rtvO, rtvN, rtvM };
    ctx->OMSetRenderTargets(4, rtvs, dsv);

    if (vp)
        ctx->RSSetViewports(1, vp);
}

void CRenderTargetManager::Clear_RenderTarget(const CRenderTarget::RTType type, const _bool _isEditor)
{
    ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

    if (!context)
        return;

    auto& rtMap = PickRTMap(this, _isEditor);
    auto it = rtMap.find(type);

    if (it == rtMap.end())
        return;

    CRenderTarget& rt = it->second;

    if (type == CRenderTarget::RTType::Depth || type == CRenderTarget::RTType::ShadowDepth)
    {
        ID3D11DepthStencilView* dsv = rt.GetDSV();

        if (!dsv)
            return;

        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        return;
    }

    float clear[4] = { 0,0,0,0 };

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

void CRenderTargetManager::Clear_GBuffer(const _bool _isEditor)
{
    Clear_RenderTarget(CRenderTarget::RTType::Albedo, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Object, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Normal, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Material, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Depth, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Diffuse, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Specular, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::ShadowMask, _isEditor);
    Clear_RenderTarget(CRenderTarget::RTType::Combine, _isEditor);
}

ID3D11Texture2D* CRenderTargetManager::GetTexture(const CRenderTarget::RTType type, const _bool _isEditor) const
{
    auto& rtMap = PickRTMapConst(this, _isEditor);
    auto it = rtMap.find(type);
    if (it == rtMap.end())
        return nullptr;
    return it->second.GetTexture();
}

ID3D11RenderTargetView* CRenderTargetManager::GetRTV(const CRenderTarget::RTType type, const _bool _isEditor) const
{
    auto& rtMap = PickRTMapConst(this, _isEditor);
    auto it = rtMap.find(type);
    if (it == rtMap.end())
        return nullptr;
    return it->second.GetRTV();
}

ID3D11ShaderResourceView* CRenderTargetManager::GetSRV(const CRenderTarget::RTType type, const _bool _isEditor) const
{
    auto& rtMap = PickRTMapConst(this, _isEditor);
    auto it = rtMap.find(type);
    if (it == rtMap.end())
        return nullptr;
    return it->second.GetSRV();
}

ID3D11DepthStencilView* CRenderTargetManager::GetDSV(const CRenderTarget::RTType type, const _bool _isEditor) const
{
    auto& rtMap = PickRTMapConst(this, _isEditor);
    auto it = rtMap.find(type);
    if (it == rtMap.end())
        return nullptr;
    return it->second.GetDSV();
}

const _uint CRenderTargetManager::GetWidth(const _bool _isEditor) const
{
    return _isEditor ? m_iWidth_E : m_iWidth;
}

const _uint CRenderTargetManager::GetHeight(const _bool _isEditor) const
{
        return _isEditor ? m_iHeight_E : m_iHeight;
}

void CRenderTargetManager::Unbind_AllSRVs_PS(ID3D11DeviceContext* context, const _bool _isEditor)
{
    if (!context)
        return;

    ID3D11ShaderResourceView* nullSRV[16] = {};
    context->PSSetShaderResources(0, 16, nullSRV);
}

HRESULT CRenderTargetManager::CreateTargets(ID3D11Device* device, _uint width, _uint height, const _bool _isEditor)
{
    if (!device || width == 0 || height == 0)
        return E_FAIL;

    if (_isEditor)
    {
        m_iWidth_E = width;
        m_iHeight_E = height;
    }
    else
    {
        m_iWidth = width;
        m_iHeight = height;
    }

    auto& rt = PickRTMap(this, _isEditor);

    if (FAILED(rt[CRenderTarget::RTType::Combine].Create(CRenderTarget::RTType::Combine, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Albedo].Create(CRenderTarget::RTType::Albedo, device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Object].Create(CRenderTarget::RTType::Object, device, width, height, DXGI_FORMAT_R32_UINT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Normal].Create(CRenderTarget::RTType::Normal, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Material].Create(CRenderTarget::RTType::Material, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Depth].Create(CRenderTarget::RTType::Depth, device, width, height, DXGI_FORMAT_R24G8_TYPELESS, true)))
        return E_FAIL;

    if (!_isEditor)
    {
        _uint shadowMapSize = CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;

        if (FAILED(rt[CRenderTarget::RTType::ShadowDepth].Create(CRenderTarget::RTType::ShadowDepth, device, shadowMapSize, shadowMapSize, DXGI_FORMAT_R32_TYPELESS, true)))
            return E_FAIL;
    }

    if (FAILED(rt[CRenderTarget::RTType::Diffuse].Create(CRenderTarget::RTType::Diffuse, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::Specular].Create(CRenderTarget::RTType::Specular, device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, true)))
        return E_FAIL;

    if (FAILED(rt[CRenderTarget::RTType::ShadowMask].Create(CRenderTarget::RTType::ShadowMask, device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true)))
        return E_FAIL;

    return S_OK;
}

map<CRenderTarget::RTType, CRenderTarget>& CRenderTargetManager::PickRTMap(CRenderTargetManager* self, _bool isEditor)
{
    return isEditor ? self->m_rtList_E : self->m_rtList;
}

const map<CRenderTarget::RTType, CRenderTarget>& CRenderTargetManager::PickRTMapConst(const CRenderTargetManager* self, _bool isEditor)
{
    return isEditor ? self->m_rtList_E : self->m_rtList;
}
