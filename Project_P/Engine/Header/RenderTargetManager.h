#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRenderTargetManager final
{
	SINGLETONCLASS(CRenderTargetManager);

public:
    HRESULT Initialize();
    void Destroy();

    HRESULT Resize(ID3D11Device* device, UINT width, UINT height, const _bool _isEditor = false);

public:
    void Bind_RenderTarget(const CRenderTarget::RTType type, ID3D11DeviceContext* context, const D3D11_VIEWPORT* vp, const _bool _isEditor = false);
    void Bind_GBuffer(ID3D11DeviceContext* ctx, const D3D11_VIEWPORT* vp, const _bool _isEditor = false);
    
    void Clear_RenderTarget(const CRenderTarget::RTType type, const _bool _isEditor = false);
    void Clear_GBuffer(const _bool _isEditor = false);

public:
    ID3D11Texture2D* GetTexture(const CRenderTarget::RTType type, const _bool _isEditor = false) const;
    ID3D11RenderTargetView* GetRTV(const CRenderTarget::RTType type, const _bool _isEditor = false) const;
    ID3D11ShaderResourceView* GetSRV(const CRenderTarget::RTType type, const _bool _isEditor = false) const;
    ID3D11DepthStencilView* GetDSV(const CRenderTarget::RTType type, const _bool _isEditor = false) const;

    const _uint GetWidth(const _bool _isEditor = false) const;
    const _uint GetHeight(const _bool _isEditor = false) const;

public:
    static void Unbind_AllSRVs_PS(ID3D11DeviceContext* context, const _bool _isEditor = false);

private:
    HRESULT CreateTargets(ID3D11Device* device, _uint width, _uint height, const _bool _isEditor = false);

public:
    static map<CRenderTarget::RTType, CRenderTarget>& PickRTMap(CRenderTargetManager* self, _bool isEditor);

    static const map<CRenderTarget::RTType, CRenderTarget>& PickRTMapConst(const CRenderTargetManager* self, _bool isEditor);

private:
    _uint m_iWidth, m_iHeight;
    _uint m_iWidth_E, m_iHeight_E;
    map<CRenderTarget::RTType, CRenderTarget> m_rtList;
    map<CRenderTarget::RTType, CRenderTarget> m_rtList_E;
};

NS_END

