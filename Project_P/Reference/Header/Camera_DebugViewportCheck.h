#pragma once
#include "epch.h"
#include <d3d11.h>
#include <string>
#include <sstream>

static inline bool GetTex2DDescFromRTV(ID3D11RenderTargetView* rtv, D3D11_TEXTURE2D_DESC& outDesc)
{
    if (!rtv) return false;

    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    if (!res) return false;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    res->Release();
    if (FAILED(hr) || !tex) return false;

    tex->GetDesc(&outDesc);
    tex->Release();
    return true;
}

static inline std::string VPToString(const D3D11_VIEWPORT* vp)
{
    if (!vp) return "VP=null";
    std::ostringstream oss;
    oss << "VP{ x=" << vp->TopLeftX
        << ", y=" << vp->TopLeftY
        << ", w=" << vp->Width
        << ", h=" << vp->Height
        << ", min=" << vp->MinDepth
        << ", max=" << vp->MaxDepth
        << " }";
    return oss.str();
}

static inline void DebugCheckRTViewport_Impl(
    const char* tag,
    ID3D11RenderTargetView* rtv,
    const D3D11_VIEWPORT* useVP)
{
    D3D11_TEXTURE2D_DESC td{};
    const bool hasRT = GetTex2DDescFromRTV(rtv, td);

    std::ostringstream oss;
    oss << "[RT VP CHECK] " << (tag ? tag : "(null)") << "\n";
    oss << "  " << VPToString(useVP) << "\n";

    if (hasRT)
    {
        oss << "  RT{ w=" << td.Width
            << ", h=" << td.Height
            << ", fmt=" << (int)td.Format
            << ", sample=" << td.SampleDesc.Count
            << " }\n";

        // “검정 띠” 조건 대충 판정: RT는 (0,0) 기준인데 VP가 y>0이면 상단이 비어버림
        const float vpY = useVP ? useVP->TopLeftY : 0.f;
        const float vpH = useVP ? useVP->Height : (float)td.Height;

        if (vpY > 0.0f)
            oss << "  !! Suspicious: VP.TopLeftY > 0 (RT는 원점이 0,0) => 상단 미렌더 가능\n";

        // 크기 불일치도 체크
        if (useVP)
        {
            const float rtW = (float)td.Width;
            const float rtH = (float)td.Height;

            if (fabsf(useVP->Width - rtW) > 0.5f || fabsf(useVP->Height - rtH) > 0.5f)
                oss << "  !! Suspicious: VP(Width/Height) != RT(Width/Height)\n";
        }

        // 상단 띠 예상 픽셀 수
        if (vpY > 0.0f)
            oss << "  예상 상단 띠(px): " << (int)(vpY + 0.5f) << "\n";
    }
    else
    {
        oss << "  RT{ desc read failed (rtv null or not Texture2D) }\n";
    }

    CDebug::Log(oss.str());
}

#define DEBUG_CHECK_RT_VIEWPORT(TAG, RTV, USEVP) \
    DebugCheckRTViewport_Impl((TAG), (RTV), (USEVP))