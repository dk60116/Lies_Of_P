// DeferredLighting_DiffuseOnly.hlsl

// 라이트 정의
#define PI 3.141592

#define MAX_LIGHTS 64

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#define METALLIC_GAMMA 0.45f  
#define METALLIC_BOOST 1.35f  
#define DIFFUSE_KILL_POWER 1.75f  
#define SPEC_BOOST 2.0f  
#define ROUGHNESS_METAL_MUL 0.35f 

cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float cpadding;
    float4x4 view;
    float4x4 proj;
};

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[64];
};

cbuffer PerCustomValue : register(b5)
{
    float4x4 gInvViewProj;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D<float> gDepth : register(t2);
Texture2D gMaterial : register(t3);
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

float3 DecodeNormal(float3 enc01)
{
    return normalize(enc01 * 2.0f - 1.0f);
}

float3 ReconstructWorldPos(float2 uv, float depth01)
{
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = uv.y * 2.0f - 1.0f;

    float4 clip = float4(ndc, depth01, 1.0f);

    float4 world = mul(clip, gInvViewProj);
    world.xyz /= world.w;

    return world.xyz;
}

float RemapMetallicArt(float m)
{
    m = saturate(m * METALLIC_BOOST);
    // gamma로 0/1쪽으로 몰기
    m = pow(m, METALLIC_GAMMA);
    // 극단 강화(선택): 중간값을 더 빠르게 밀어줌
    m = smoothstep(0.05f, 0.95f, m);
    return m;
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    // UE4 스타일 k
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / max(NdotX * (1.0f - k) + k, 1e-6f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // pow(1 - cosTheta, 5)
    float f = pow(saturate(1.0f - cosTheta), 5.0f);
    return F0 + (1.0f - F0) * f;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvScreen = i.uv;
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    // GBuffer fetch
    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);
    
    if (depth01 >= 0.999999f)
        return float4(0, 0, 0, 1);

    float3 N = DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz);

    float3 albedo = gAlbedo.Sample(gSampler, uvTex).rgb;
    albedo = saturate(albedo);

    float4 mat = gMaterial.Sample(gSampler, uvTex);
    
    float occulusion = saturate(mat.r);
    float roughness = saturate(mat.g);
    float metallic = saturate(mat.b);

    // World position / view vector
    float3 posW = ReconstructWorldPos(uvScreen, depth01);
    float3 V = normalize(camPos - posW);

    float NdotV = saturate(dot(N, V));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 Lo = 0;

    int lightCount = (int) gLight[0][3][3];
    
    int dirLightCount = 0;

    [loop]
    for (int li = 0; li < lightCount; ++li)
    {
        if (gLight[li][3][2] < 0.5f)
            continue;

        uint lightType = (uint) gLight[li][3][0];
        float3 lightPos = float3(gLight[li][0][0], gLight[li][0][1], gLight[li][0][2]);
        float3 lightDir = float3(gLight[li][1][0], gLight[li][1][1], gLight[li][1][2]);
        float3 lightCol = float3(gLight[li][2][0], gLight[li][2][1], gLight[li][2][2]);

        float intensity = gLight[li][1][3];
        float range = gLight[li][0][3];

        float3 L = 0;
        float att = 1.0f;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-lightDir);
            att = 1.0f;
            
            ++dirLightCount;
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            float3 toL = lightPos - posW;
            float distSq = dot(toL, toL);
            float rangeSq = range * range;

            if (range <= 1e-6f || distSq >= rangeSq)
                continue;

            float dist = sqrt(distSq);
            L = toL / max(dist, 1e-6f);

            float falloff = saturate(1.0f - dist / range);
            falloff *= falloff;

            float invSqNorm = rangeSq / max(distSq, 1e-3f);
            invSqNorm = min(invSqNorm, 16.0f);

            att = falloff * invSqNorm;
        }
        else
        {
            continue;
        }

        float NdotL = saturate(dot(N, L));
        if (NdotL <= 1e-6f)
            continue;

        float3 H = normalize(V + L);
        float NdotH = saturate(dot(N, H));
        float VdotH = saturate(dot(V, H));

        // Radiance
        float3 radiance = lightCol * (intensity * att);

        // Cook-Torrance BRDF
        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        float3 F = FresnelSchlick(VdotH, F0);

        float3 spec = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-6f);

        // Diffuse: metallic일수록 줄어듦 + 에너지 보존(F) 반영
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metallic);

        float3 diffuse = kD * (albedo);

        Lo += (diffuse + spec) * radiance * NdotL;
    }
    
    float ambient = dirLightCount ? 0.5f : 0.2f;
    
    float3 ambientV = ambient * albedo * (1.0f - (metallic * 0.7f));

    float3 color = ambientV + Lo;
    color = saturate(color);

    return float4(color, 1);
}