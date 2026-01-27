// ───────────── 상수 버퍼
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

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint useTexture;
    uint boneCount;
    float2 mpadding;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

// ───────────── 텍스처 및 샘플러
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);
// ───────────── 버텍스 입력

struct VSIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangentL : TANGENT;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

// ───────────── 버텍스 출력
struct VSOut
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// ───────────── 버텍스 셰이더
VSOut VSMain(VSIn v)
{
    VSOut o;
    
    float4 skinnedPos = float4(0, 0, 0, 0);
    
    if (boneCount)
    {
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            uint idx = v.boneIndices[i];
            float w = v.boneWeights[i];
            skinnedPos += mul(float4(v.posL, 1.0f), gBones[idx]) * w;
        }
    }
    else
        skinnedPos = float4(v.posL, 1.0f);

    float4 posW = mul(skinnedPos, world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);

    o.uv = v.uv;
    return o;
}

// ───────────── 픽셀 셰이더
float4 PSMain(VSOut input) : SV_TARGET
{
    if (useTexture != 0)
    {
        float4 texColor = gTexture.Sample(gSampler, input.uv);
        return texColor * baseColor;
    }
    else
        return baseColor;
}