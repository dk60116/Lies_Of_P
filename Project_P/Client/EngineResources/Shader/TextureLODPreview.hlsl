cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float cPadding;
    float4x4 view;
    float4x4 proj;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint useTexture;
    uint useNormalMap;
    uint useORMMap;
    uint boneCount;
    uint useAlphaMap;
    uint materialPadding0;
    uint materialPadding1;
    uint materialPadding2;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[512];
};

cbuffer PerInstance : register(b4)
{
    float4x4 gInstanceWorlds[128];
    float4 gInstanceFillParams[128];
    uint gInstanceCount;
    float3 gInstancePadding;
};

cbuffer PerCustomValue : register(b10)
{
    float4 gPreviewParams;
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
    float3 normalW : TEXCOORD1;
};

struct PSOut
{
    float4 Albedo : SV_Target0;
    uint Object : SV_Target1;
    float4 Normal : SV_Target2;
    float4 Material : SV_Target3;
};

VSOut VSMain(VSIn v, uint instanceID : SV_InstanceID)
{
    VSOut o;

    float4 skinnedPos = float4(v.posL, 1.0f);
    float3 skinnedN = v.normalL;

    if (boneCount != 0)
    {
        skinnedPos = 0;
        skinnedN = 0;

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            const float weight = v.boneWeights[i];
            const uint boneIndex = v.boneIndices[i];

            if (weight > 0.0f && boneIndex < 512)
            {
                const float4x4 boneMatrix = gBones[boneIndex];
                skinnedPos += mul(float4(v.posL, 1.0f), boneMatrix) * weight;
                skinnedN += mul(v.normalL, (float3x3)boneMatrix) * weight;
            }
        }
    }

    float4x4 worldMat = world;
    if (gInstanceCount > 0 && instanceID < gInstanceCount)
        worldMat = gInstanceWorlds[instanceID];

    const float4 posW = mul(skinnedPos, worldMat);
    const float4 posV = mul(posW, view);

    o.posH = mul(posV, proj);
    o.uv = v.uv;
    o.normalW = normalize(mul(skinnedN, (float3x3)worldMat));
    return o;
}

PSOut PSMain(VSOut input)
{
    PSOut o;

    const float2 uv = input.uv * gPreviewParams.yz;
    const float previewMip = max(gPreviewParams.x, 0.0f);
    const float4 texColor = (useTexture != 0) ? gTexture.SampleLevel(gSampler, uv, previewMip) : float4(1.0f, 1.0f, 1.0f, 1.0f);

    o.Albedo = saturate(baseColor * texColor);
    o.Object = 0u;

    const float3 normalW = normalize(input.normalW);
    o.Normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    o.Material = float4(1.0f, 0.5f, 0.0f, 1.0f);
    return o;
}
