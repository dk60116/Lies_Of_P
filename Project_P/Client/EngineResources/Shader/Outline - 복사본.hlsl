// ───────────── 상수 버퍼
cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float4x4 view;
    float4x4 proj;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint boneCount;
    float2 padding; // 16바이트 정렬
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

float mask[9] =
{
    -1, -1, -1,
    -1, 8, -1,
    -1, -1, -1
};

float coord[3] = { -1, 0, 1 };
float divider = 1;

struct VSIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangentL : TANGENT;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
    float lineWidth : VALUE;
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
    float3 skinnedNormal = float3(0, 0, 0);
    
    if (boneCount)
    {
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            uint idx = v.boneIndices[i];
            float w = v.boneWeights[i];
            skinnedPos += mul(float4(v.posL, 1.0f), gBones[idx]) * w;
            skinnedNormal += mul(float4(v.normalL, 0), gBones[idx]).xyz * w;
        }
        
        skinnedNormal = normalize(skinnedNormal);
    }
    else
    {
        skinnedPos = float4(v.posL, 1.0f);
        skinnedNormal = normalize(v.normalL);
    }

    float4 posW = mul(skinnedPos, world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    
    skinnedPos.xyz += skinnedNormal * v.lineWidth;

    o.uv = v.uv;
    return o;
}

Texture2D myTexture : register(t0);
SamplerState mySampler : register(s0);

// ───────────── 픽셀 셰이더
float4 PSMain(VSOut input) : SV_TARGET
{
    //float4 Color = 0;
    //float4 Ret = 0;
    //float3 grayScale = float3(0.3, 0.59, 0.11);
    
    //for (int i = 0; i < 9; ++i)
    //    color += mask[i] * (gNmormalW.Sample(samLinear, PrimitiveIndex.Tex + float2(coord[i % 3] / MAP_CX, coord[i / 3] / MAP_CY)));
    
    //float gray = 1 - dot(Color, grayScale);
    
    //Ret = float4(gray, gray, gray, 1) / divider;
    
    return baseColor;
}