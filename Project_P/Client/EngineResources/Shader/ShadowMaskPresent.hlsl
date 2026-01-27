// ShadowMaskPresent.hlsl
// 목적: SRV(t0)의 R 채널(0~1)을 그레이스케일로 표시

cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 pos; // 사용 안 함(바인딩 호환)
    float4x4 view;
    float4x4 proj;
    float cpadding;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    float4 posW = mul(float4(v.posL, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.uv = v.uv;
    return o;
}

float4 PSMain(VSOut input) : SV_Target
{
    // 엔진이 현재 Present에서 y-flip 쓰는 패턴이면 동일하게 맞추세요.
    float2 uv = float2(input.uv.x, 1.0f - input.uv.y);

    float s = gTexture.Sample(gSampler, uv).r; // ShadowMask in R
    return float4(s, s, s, 1.0f);
}
