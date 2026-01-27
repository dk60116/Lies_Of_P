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

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION; // VertexTexNormalTangentBuffer.position
    float2 uv : TEXCOORD0; // VertexTexNormalTangentBuffer.uv
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
    uv.y = 1.0f - uv.y; // 상하 플립

    float4 c = gTexture.Sample(gSampler, uv);
    c.a = 1.0f;
    return c;
}
