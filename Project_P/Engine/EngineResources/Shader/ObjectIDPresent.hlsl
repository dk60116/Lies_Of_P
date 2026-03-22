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

Texture2D<uint> gObjectTex : register(t0);
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

uint Hash32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float3 IdToColor(uint id)
{
    if (id == 0)
        return float3(0, 0, 0);

    uint h = Hash32(id);
    uint r = (h) & 0xFF;
    uint g = (h >> 8) & 0xFF;
    uint b = (h >> 16) & 0xFF;

    r = max(r, 64u);
    g = max(g, 64u);
    b = max(b, 64u);

    return float3(r, g, b) / 255.0f;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uv = i.uv;
    uv.y = 1.f - uv.y;
    
    uint w, h;
    gObjectTex.GetDimensions(w, h);

    int2 pix = int2(uv * float2(w, h));
    pix = clamp(pix, int2(0, 0), int2(int(w) - 1, int(h) - 1));

    uint id = gObjectTex.Load(int3(pix, 0));
    return float4(IdToColor(id), 1);
}