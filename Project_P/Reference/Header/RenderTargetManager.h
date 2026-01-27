#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRenderTargetManager final
{
	SINGLETONCLASS(CRenderTargetManager);

public:
    HRESULT Initialize();
    void Destroy();

    HRESULT Resize(ID3D11Device* device, UINT width, UINT height);

public:
    void Bind_RenderTarget(const CRenderTarget::RTType type, ID3D11DeviceContext* context, const D3D11_VIEWPORT* vp);
    void Bind_GBuffer(ID3D11DeviceContext* ctx, const D3D11_VIEWPORT* vp);
    
    void Clear_RenderTarget(const CRenderTarget::RTType type);
    void Clear_GBuffer();

public:
    ID3D11RenderTargetView* GetRTV(const CRenderTarget::RTType type) const;
    ID3D11ShaderResourceView* GetSRV(const CRenderTarget::RTType type) const;
    ID3D11DepthStencilView* GetDSV(const CRenderTarget::RTType type) const;

    _uint GetWidth() const { return m_width; }
    _uint GetHeight() const { return m_height; }

public:
    static void Unbind_AllSRVs_PS(ID3D11DeviceContext* context);

private:
    HRESULT CreateTargets(ID3D11Device* device, _uint width, _uint height);

private:
    _uint m_width;
    _uint m_height;

    map<CRenderTarget::RTType, CRenderTarget> m_rtList;
};

NS_END

