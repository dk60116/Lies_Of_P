#define PI 3.14159265359
#define MAX_LIGHTS 64

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1

cbuffer PerObject : register(b0)
{
    float4x4 world; // fullscreen quad world
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float cpadding0;
    float4x4 view;
    float4x4 proj;
};

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[MAX_LIGHTS];
};

#pragma pack_matrix(row_major)
cbuffer PerCustomValue : register(b5)
{
    float4x4 gInvViewProj;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D<float> gDepth : register(t2);
Texture2D gMaterial : register(t3);
SamplerState gSampler : register(s0); // <- s4 ¾²Áö ¸»°í s0 ±ÇÀå

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

float3 ReconstructWorldPos(float2 uvScreen, float depth01)
{
    float2 ndc = float2(uvScreen.x * 2.0f - 1.0f, uvScreen.y * 2.0f - 1.0f);
    float4 clip = float4(ndc, depth01, 1.0f);
    float4 w = mul(clip, gInvViewProj);
    return w.xyz / max(w.w, 1e-6f);
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = pow(saturate(roughness), 1.5f);
    a = max(a, 1e-4f);
    float a2 = a * a;

    float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / max(NdotX * (1.0f - k) + k, 1e-6f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float f = pow(saturate(1.0f - cosTheta), 5.0f);
    return F0 + (1.0f - F0) * f;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvScreen = i.uv;
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);
    if (depth01 >= 0.999999f)
        return float4(0, 0, 0, 1);

    float3 N = DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz);

    float3 albedo = saturate(gAlbedo.Sample(gSampler, uvTex).rgb);

    float4 mat = gMaterial.Sample(gSampler, uvTex);
    
    float occulusion = saturate(mat.r);
    float roughness = saturate(mat.g);
    float metallic = saturate(mat.b);
    
    roughness = roughness * (1.f + metallic * 0.5f);

    float3 posW = ReconstructWorldPos(uvScreen, depth01);
    float3 V = normalize(camPos - posW);

    float NdotV = saturate(dot(N, V));
    if (NdotV <= 1e-6f)
        return float4(0, 0, 0, 1);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 specSum = 0;

    int lightCount = clamp((int) gLight[0][3][3], 0, MAX_LIGHTS);

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

        float3 radiance = lightCol * (intensity * att);

        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        float3 F = FresnelSchlick(VdotH, F0);

        float3 spec = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-6f);

        specSum += spec * radiance * NdotL;
    }

    return float4(specSum, 1);
}