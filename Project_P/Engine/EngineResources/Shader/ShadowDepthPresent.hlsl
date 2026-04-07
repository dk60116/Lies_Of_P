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

Texture2DArray<float> gShadowDepth : register(t0);
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
    float2 atlasUV = saturate(i.uv);
    float2 tiledUV = atlasUV * 2.0f;
    uint tileX = min((uint)tiledUV.x, 1u);
    uint tileY = min((uint)tiledUV.y, 1u);
    uint cascadeIndex = tileX + tileY * 2u;

    float2 uvTex = frac(tiledUV);
    uvTex.y = 1.0f - uvTex.y;

    float d = gShadowDepth.SampleLevel(gSampler, float3(uvTex, cascadeIndex), 0);

    float v = RemapDepthForView(d);
    
    return float4(v, v, v, 1.0f);
}
