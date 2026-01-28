cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 pos;
    float4x4 view;
    float4x4 proj;
    float cpadding;
};

Texture2D gObjectTex : register(t0);
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
    float4 posW = mul(float4(v.posL, 1), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.uv = v.uv;
    return o;
}

uint DecodeID24(float3 rgb01)
{
    uint3 c = (uint3)round(saturate(rgb01) * 255.0f);
    return c.x | (c.y << 8) | (c.z << 16);
}

float3 hsv2rgb(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

float3 DebugColorFromID(uint id)
{
    float h = frac((float) id * 0.61803398875);
    return hsv2rgb(float3(h, 0.95, 1.0));
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uv = i.uv;
    uv.y = 1.0f - uv.y;
    
    float3 rgb01 = gObjectTex.Sample(gSampler, uv).rgb;
    
    uint id = DecodeID24(rgb01);
    float3 dbg = DebugColorFromID(id);

    if (id == 0)
        dbg = 0;

    return float4(dbg, 1);
}