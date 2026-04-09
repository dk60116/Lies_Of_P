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
    float4x4 gBones[512];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness;
    float2 gTiling;
    float2 gOffset;
};

#define MAX_LIGHTS 64
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2
#define DEFAULT_SPECULAR_POWER 80.0f
#define DEFAULT_SPECULAR_BOOST 1.8f

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[MAX_LIGHTS];
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
    float3 normalW : NORMAL;
    float3 posW : TEXCOORD1;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    float4 skinnedPos = float4(v.posL, 1);
    if (boneCount)
    {
        skinnedPos = 0;
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];
            skinnedPos += mul(float4(v.posL, 1), gBones[idx]) * w;
        }
    }

    float3 skinnedN = v.normalL;
    if (boneCount)
    {
        skinnedN = 0;
        [loop]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];
            skinnedN += mul((float3x3)gBones[idx], v.normalL) * w;
        }
    }

    float4 posW = mul(skinnedPos, world);
    float3 normalW = normalize(mul((float3x3)world, skinnedN));

    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.posW = posW.xyz;
    o.normalW = normalW;
    o.uv = v.uv;
    return o;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float2 tillingUV = float2(input.uv.x * gTiling.x + gOffset.x * gTiling.x + gOffset.x, input.uv.y * gTiling.y + gOffset.y);
    float4 texColor = useTexture ? gTexture.Sample(gSampler, tillingUV) : float4(1, 1, 1, 1);

    float3 N = normalize(input.normalW);
    float3 V = normalize(pos - input.posW);

    float3 diffuseSum = float3(0, 0, 0);
    float3 ambientSum = float3(0, 0, 0);
    float3 specularSum = float3(0, 0, 0);

    int lightCount = clamp((int)gLight[0][3][3], 0, MAX_LIGHTS - 1);

    for (int i = 1; i <= lightCount; ++i)
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
        float spotCos = gLight[i][3][3];

        float3 L;
        float attenuation = 1.0f;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-lightDir);
        }
        else if (lightType == LIGHT_TYPE_POINT || lightType == LIGHT_TYPE_SPOT)
        {
            float3 toLight = lightPos - input.posW;
            float dist = length(toLight);
            if (range <= 0.0001f || dist >= range)
                continue;

            L = toLight / max(dist, 0.0001f);
            attenuation = saturate(1.0f - dist / max(range, 0.0001f)) * attenuationK;

            if (lightType == LIGHT_TYPE_SPOT)
            {
                float coneCos = dot(normalize(-L), normalize(lightDir));
                if (coneCos <= spotCos)
                    continue;

                float coneAtt = saturate((coneCos - spotCos) / max(1.0f - spotCos, 1e-4f));
                attenuation *= coneAtt * coneAtt;
            }
        }
        else
        {
            continue;
        }

        float NdotL = saturate(dot(N, L));
        float3 diffuse = lightColor * NdotL * intensity * attenuation;
        diffuseSum += diffuse;

        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        float smoothness = saturate(gSmoothness);
        float specularPower = lerp(18.0f, DEFAULT_SPECULAR_POWER, smoothness);
        float3 fSpecular = pow(RdotV, specularPower);
        float specularStrength = smoothness * DEFAULT_SPECULAR_BOOST;
        float3 specular = lightColor * fSpecular * specularStrength * intensity * attenuation;
        specularSum += specular;

        float3 ambient = lightColor * ambientK;
        ambientSum += ambient;
    }

    diffuseSum = max(diffuseSum, float3(0.1f, 0.1f, 0.1f));
    float3 litDiffuse = texColor.rgb * saturate(ambientSum + diffuseSum);
    float3 finalColor = saturate(litDiffuse + specularSum);
    return float4(finalColor, texColor.a);
}
