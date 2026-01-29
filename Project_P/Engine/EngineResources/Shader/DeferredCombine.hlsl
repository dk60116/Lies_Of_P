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

Texture2D gAlbedo : register(t0);
Texture2D<float> gDepth : register(t1);
Texture2D gShading : register(t2);
Texture2D gSpecular : register(t3);
Texture2D<float> gShadow : register(t4);
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
    float2 uv = input.uv;
    uv.y = 1.0f - uv.y;

    float4 albedo = gAlbedo.Sample(gSampler, uv);
    float depth = gDepth.SampleLevel(gSampler, uv, 0);
    float shadowF = gShadow.Sample(gSampler, uv);
    float4 diffuse = gShading.Sample(gSampler, uv);
    float4 specualr = gSpecular.Sample(gSampler, uv);
    
    if (depth >= 0.999999f)
        return albedo;
    
    float shadowValue = (1.f - shadowF) * 0.3f;
    float4 shadow = float4(shadowValue, shadowValue, shadowValue, 0.f);
    
    float4 ad = diffuse;
    float4 ads = ad + specualr;
    float4 adss = ads - shadow;
    
    return adss;
}