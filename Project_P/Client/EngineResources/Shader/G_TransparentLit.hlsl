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
    float4x4 gInvViewProj;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint useTexture;
    uint useNormalMap;
    uint useORMMap;
    uint boneCount;
    uint useAlphaMap;
    uint3 mpadding;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[512];
};

cbuffer PerCustomValue : register(b10)
{
    float gOcculusion;
    float gRoughness;
    float gMetallic;
    uint gObjectID;
    float2 gTiling;
    float2 gOffset;
};

#define MAX_LIGHTS 64

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[MAX_LIGHTS];
};

Texture2D gTexture : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gORMMap : register(t2);
Texture2D gAlphaMap : register(t3);
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
    float3 posW : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normalW : TEXCOORD2;
    float3 tangentW : TEXCOORD3;
    float3 bitanW : TEXCOORD4;
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    float4 skinnedPos = float4(v.posL, 1.0f);
    float3 skinnedN = v.normalL;
    float3 skinnedT = v.tangentL;

    if (boneCount != 0)
    {
        skinnedPos = 0;
        skinnedN = 0;
        skinnedT = 0;

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];

            if (w > 0.0f && idx < 512)
            {
                float4x4 M = gBones[idx];
                skinnedPos += mul(float4(v.posL, 1.0f), M) * w;
                skinnedN += mul(v.normalL, (float3x3)M) * w;
                skinnedT += mul(v.tangentL, (float3x3)M) * w;
            }
        }
    }

    float4 posW4 = mul(skinnedPos, world);

    float3 N = normalize(mul(skinnedN, (float3x3)world));
    float3 T = normalize(mul(skinnedT, (float3x3)world));

    T = normalize(T - N * dot(T, N));
    float3 B = normalize(cross(N, T));

    float4 posV = mul(posW4, view);
    o.posH = mul(posV, proj);
    o.posW = posW4.xyz;
    o.uv = v.uv;
    o.normalW = N;
    o.tangentW = T;
    o.bitanW = B;

    return o;
}

float4 PSMain(VSOut input) : SV_Target
{
    float2 uv = input.uv * gTiling + gOffset;
    float4 texColor = (useTexture != 0) ? gTexture.Sample(gSampler, uv) : float4(1, 1, 1, 1);
    float alphaMask = (useAlphaMap != 0) ? gAlphaMap.Sample(gSampler, uv).r : 1.0f;

    float3 N = normalize(input.normalW);
    if (useNormalMap != 0)
    {
        float3 nTS = gNormalMap.Sample(gSampler, uv).rgb * 2.0f - 1.0f;
        float3 T = normalize(input.tangentW);
        float3 B = normalize(input.bitanW);
        N = normalize(nTS.x * T + nTS.y * B + nTS.z * N);
    }

    float occ = gOcculusion;
    float roughness = gRoughness;
    float metallic = gMetallic;

    if (useORMMap != 0)
    {
        float3 orm = gORMMap.Sample(gSampler, uv).rgb;
        occ = orm.r;
        roughness = orm.g;
        metallic = orm.b;
    }

    float3 V = normalize(camPos - input.posW);
    float3 diffuseSum = float3(0, 0, 0);
    float3 ambientSum = float3(0, 0, 0);
    float3 specularSum = float3(0, 0, 0);

    int lightCount = (int)gLight[0][3][3];
    float specularPower = lerp(8.0f, 128.0f, 1.0f - saturate(roughness));
    float specularStrength = 1.0f - saturate(roughness);

    for (int i = 0; i < lightCount; ++i)
    {
        if (gLight[i][3][2] < 0.5f)
            continue;

        uint lightType = (uint)gLight[i][3][0];
        float3 lightPos = float3(gLight[i][0][0], gLight[i][0][1], gLight[i][0][2]);
        float3 lightDir = float3(gLight[i][1][0], gLight[i][1][1], gLight[i][1][2]);
        float3 lightColor = float3(gLight[i][2][0], gLight[i][2][1], gLight[i][2][2]);
        float intensity = gLight[i][1][3];
        float range = gLight[i][0][3];
        float attenuationK = gLight[i][3][1];
        float ambientK = gLight[i][2][3];

        float3 L;
        float attenuation = 1.0f;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-lightDir);
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            float3 toLight = lightPos - input.posW;
            float dist = length(toLight);
            L = toLight / max(dist, 0.0001f);
            attenuation = saturate(1.0f - dist / max(range, 0.0001f)) * attenuationK;
        }
        else
            continue;

        float NdotL = saturate(dot(N, L));
        float3 diffuse = lightColor * NdotL * intensity * attenuation;
        diffuseSum += diffuse;

        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        float3 specular = lightColor * pow(NdotH, specularPower) * specularStrength * intensity * attenuation;
        specularSum += specular;

        ambientSum += lightColor * ambientK;
    }

    diffuseSum = max(diffuseSum, float3(0.1f, 0.1f, 0.1f));
    float3 albedo = saturate(baseColor.rgb * texColor.rgb);
    float3 litDiffuse = albedo * saturate(ambientSum + diffuseSum) * occ;
    float3 finalColor = saturate(litDiffuse + specularSum * (1.0f - metallic));

    float finalAlpha = saturate(baseColor.a * alphaMask);
    return float4(finalColor, finalAlpha);
}
