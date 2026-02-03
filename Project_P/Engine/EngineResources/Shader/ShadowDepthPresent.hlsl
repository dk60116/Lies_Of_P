cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 pos;
    float _pad0;
    float4x4 view;
    float4x4 proj;
};

Texture2D<float> gShadowDepth : register(t0);
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

float RemapDepthForView(float d)
{
    d = saturate(d);

    return 1.0f - d;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float d = gShadowDepth.SampleLevel(gSampler, uvTex, 0);

    float v = RemapDepthForView(d);
    
    return float4(v, v, v, 1.0f);
}
