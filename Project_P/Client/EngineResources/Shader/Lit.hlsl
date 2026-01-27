// 상수 버퍼
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
    float4 baseColor; // rgba 0~1
    uint useTexture;
    uint boneCount;
    float2 mpadding;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness;
    float2 gTiling;
    float2 gOffset;
};

// 라이트 정의
#define MAX_LIGHTS 64

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[MAX_LIGHTS];
};

// 텍스처 & 샘플러
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

// 버텍스 입출력
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

// 버텍스 셰이더
VSOut VSMain(VSIn v)
{
    VSOut o;

    // 스킨 포지션
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

    // 스킨 노멀
    float3 skinnedN = v.normalL;
    if (boneCount)
    {
        skinnedN = 0;
        [loop]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];
            skinnedN += mul((float3x3) gBones[idx], v.normalL) * w;
        }
    }

    // 월드 변환
    float4 posW = mul(skinnedPos, world);
    float3 normalW = normalize(mul((float3x3) world, skinnedN));

    // MVP
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);

    // 출력
    o.posW = posW.xyz;
    o.normalW = normalW;
    o.uv = v.uv;
    
    return o;
}

// 픽셀 셰이더
float4 PSMain(VSOut input) : SV_TARGET
{    
    float2 tillingUV = float2(input.uv.x * gTiling.x + gOffset.x* gTiling.x + gOffset.x, input.uv.y * gTiling.y + gOffset.y);
    float4 texColor = useTexture ? gTexture.Sample(gSampler, tillingUV) : float4(1, 1, 1, 1);

    float3 N = normalize(input.normalW);
    float3 V = normalize(pos - input.posW);
    
    float3 diffuseSum = float3(0, 0, 0);
    float3 ambientSum = float3(0, 0, 0);
    float3 specularSum = float3(0, 0, 0);

    int lightCount = (int) gLight[0][3][3];

    for (int i = 0; i < lightCount; ++i)
    {
        if (gLight[i][3][2] < 0.5)
            continue;

        uint lightType = (uint) gLight[i][3][0];
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
            L = toLight / dist;
            attenuation = saturate(1.0f - dist / range) * attenuationK;
        }
        else
            continue;

        // Diffuse
        float NdotL = saturate(dot(N, L));
        float3 diffuse = lightColor * NdotL * intensity * attenuation;
        diffuseSum += diffuse;
        
        // Specular
        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        float3 fSpecular = pow(RdotV, 50);
        float specularStrength = gSmoothness;
        float3 specular = lightColor * fSpecular * specularStrength * intensity * attenuation;
        specularSum += specular;
        
        // Ambient
        float3 ambient = lightColor * ambientK;
        ambientSum += ambient;  
    }

    diffuseSum = max(diffuseSum, float3(0.1f, 0.1f, 0.1f));
    float3 litDiffuse = texColor.rgb * saturate(ambientSum + diffuseSum);
    float3 finalColor = litDiffuse + specularSum;
    
    finalColor = saturate(finalColor);

    return float4(finalColor, texColor.a);
}