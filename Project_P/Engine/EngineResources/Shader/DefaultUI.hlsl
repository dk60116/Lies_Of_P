cbuffer PerObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer PerCamera : register(b1)
{
    float3 gPos;
    float4x4 gView;
    float4x4 gProj;
    float cpadding;
}

cbuffer PerMaterial : register(b2)
{
    float4 gBaseColor;
    uint useTexture;
    uint boneCount;
    float2 padding;
}

cbuffer PerFillAmount : register(b3)
{
    float gFillAmount;
}

// ───── 버텍스 구조
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

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

// ───── Vertex Shader
VSOut VSMain(VSIn input)
{
    VSOut output;

    float4 worldPos = mul(float4(input.posL, 1.0f), gWorld);
    float4 viewPos = mul(worldPos, gView);
    output.posH = mul(viewPos, gProj);
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float4 resultColor = (useTexture != 0) ? gTexture.Sample(gSampler, input.uv) * gBaseColor : gBaseColor;
    
    if (resultColor.a < 0.01f)
        discard;
   
    if (input.uv.x >= gFillAmount)
        discard;
    
    return resultColor;
}
