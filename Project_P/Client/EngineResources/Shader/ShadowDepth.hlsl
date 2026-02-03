cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float4x4 view;
    float4x4 proj;

    float4x4 gInvViewProj;

    float cPadding;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint useTexture;
    uint useNormalMap;
    uint useORMMap;
    uint boneCount;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[512];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness;
    float2 gTiling;
    float2 gOffset;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangentL : TANGENT;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOut
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    float4 skinnedPos = float4(v.posL, 1.0f);

    if (boneCount != 0)
    {
        skinnedPos = 0;

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];

            if (w > 0.0f && idx < 512)
            {
                float4x4 M = gBones[idx];
                skinnedPos += mul(float4(v.posL, 1.0f), M) * w;
            }
        }
    }

    float4 posW = mul(skinnedPos, world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);

    o.uv = v.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return 0;
}
