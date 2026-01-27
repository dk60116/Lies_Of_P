#pragma once
#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRenderTarget final
{
public:
    enum class RTType
    {
        Defalut,
        ShadowDepthPresent,
        ShadowMaskPresent,
        Combine,
        Albedo,
        Normal,
        Material,
        Depth,
        ShadowDepth,
        Shading,
        Specular,
        ShadowMask
    };

public:
    CRenderTarget();
    ~CRenderTarget();

public:
    HRESULT Create
    (
        RTType type,
        ID3D11Device* device,
        _uint width,
        _uint height,
        DXGI_FORMAT format,
        bool createSRV = true
    );

    void Destroy();

public:
    RTType GetType() const;
    vector2Int GetWidthHeight() const;
    DXGI_FORMAT GetFormat() const;
    bool IsCreateSRV() const;

    ID3D11Texture2D* GetTexture() const;
    ID3D11RenderTargetView* GetRTV() const;
    ID3D11DepthStencilView* GetDSV() const;
    ID3D11ShaderResourceView* GetSRV() const;

    const bool IsDepth() const;
    const bool HasRTV() const;
    const bool HasDSV() const;
    const bool HasSRV() const;

private:
    HRESULT CreateColor_Internal(ID3D11Device* device);
    HRESULT CreateDepth_Internal(ID3D11Device* device);

    static bool GetDepthTypelessFormats
    (
        DXGI_FORMAT dsvFormat,
        DXGI_FORMAT& outTypelessTexFormat,
        DXGI_FORMAT& outDsvFormat,
        DXGI_FORMAT& outSrvFormat
    );

private:
    RTType m_type;
    _uint m_width;
    _uint m_height;
    DXGI_FORMAT m_format;
    bool m_createSRV;

    ID3D11Texture2D* m_texture;
    ID3D11RenderTargetView* m_rtv;
    ID3D11DepthStencilView* m_dsv;
    ID3D11ShaderResourceView* m_srv;
};

NS_END
