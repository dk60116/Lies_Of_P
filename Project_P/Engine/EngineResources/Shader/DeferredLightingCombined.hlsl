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

#pragma pack_matrix(row_major)
cbuffer InvViewProjCB : register(b5)
{
    float4x4 gInvViewProj;
};

cbuffer ShadowCB : register(b6)
{
    float4x4 gShadowViewProj;
    float2 gShadowInvMapSize;
    float gShadowBias;
    float _padShadow0;
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D<float> gDepth : register(t2);
Texture2D gMaterial : register(t3);
Texture2D<float> gShadowDepth : register(t4);
SamplerState gSampler : register(s0);

#define PI 3.14159265359
#define MAX_LIGHTS 64
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

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
    float4 posW = mul(float4(v.posL, 1.0f), world);
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
    float4 worldPos = mul(clip, gInvViewProj);
    return worldPos.xyz / max(worldPos.w, 1e-6f);
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

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 oneMinusRoughness = float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness);
    float3 fresnelMax = max(oneMinusRoughness, F0);
    float f = pow(saturate(1.0f - cosTheta), 5.0f);
    return F0 + (fresnelMax - F0) * f;
}

float SampleShadowPCF(float2 uv, float receiverDepth)
{
    if (gShadowInvMapSize.x <= 0.0f || gShadowInvMapSize.y <= 0.0f)
        return 1.0f;

    float sum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 duv = float2(x, y) * gShadowInvMapSize;
            float sd = gShadowDepth.SampleLevel(gSampler, uv + duv, 0);
            sum += (sd + gShadowBias < receiverDepth) ? 0.0f : 1.0f;
        }
    }

    return sum / 9.0f;
}

float ComputeShadow(float3 posW)
{
    if (gShadowInvMapSize.x <= 0.0f || gShadowInvMapSize.y <= 0.0f)
        return 1.0f;

    float4 posL = mul(float4(posW, 1.0f), gShadowViewProj);
    float3 ndcL = posL.xyz / max(posL.w, 1e-6f);

    float2 uvL = float2(ndcL.x * 0.5f + 0.5f, -ndcL.y * 0.5f + 0.5f);
    float depthL = ndcL.z;

    if (uvL.x < 0.0f || uvL.x > 1.0f || uvL.y < 0.0f || uvL.y > 1.0f || depthL < 0.0f || depthL > 1.0f)
        return 1.0f;

    return SampleShadowPCF(uvL, depthL);
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvScreen = i.uv;
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float4 albedoSample = gAlbedo.Sample(gSampler, uvTex);
    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);

    if (depth01 >= 0.999999f)
        return albedoSample;

    float3 albedo = saturate(albedoSample.rgb);
    float3 N = DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz);

    float4 mat = gMaterial.Sample(gSampler, uvTex);
    float occulusion = saturate(mat.r);
    float roughness = saturate(mat.g);
    float metallic = saturate(mat.b);
    float roughnessSpecular = max(roughness, 0.04f);

    float3 posW = ReconstructWorldPos(uvScreen, depth01);
    float3 V = normalize(camPos - posW);
    float NdotV = saturate(dot(N, V));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 Lo = 0.0f;
    int lightCount = clamp((int)gLight[0][3][3], 0, MAX_LIGHTS - 1);
    int dirLightCount = 0;

    [loop]
    for (int li = 1; li <= lightCount; ++li)
    {
        if (gLight[li][3][2] < 0.5f)
            continue;

        uint lightType = (uint)gLight[li][3][0];
        float3 lightPos = float3(gLight[li][0][0], gLight[li][0][1], gLight[li][0][2]);
        float3 lightDir = float3(gLight[li][1][0], gLight[li][1][1], gLight[li][1][2]);
        float3 lightCol = float3(gLight[li][2][0], gLight[li][2][1], gLight[li][2][2]);

        float intensity = gLight[li][1][3];
        float range = gLight[li][0][3];
        float attenK = gLight[li][3][1];
        float spotCos = gLight[li][3][3];

        float3 L = 0.0f;
        float att = 1.0f;

        if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            L = normalize(-lightDir);
            ++dirLightCount;
        }
        else if (lightType == LIGHT_TYPE_POINT || lightType == LIGHT_TYPE_SPOT)
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
            att = falloff * invSqNorm * max(attenK, 0.0f);

            if (lightType == LIGHT_TYPE_SPOT)
            {
                float coneCos = dot(normalize(-L), normalize(lightDir));
                if (coneCos <= spotCos)
                    continue;

                float coneAtt = saturate((coneCos - spotCos) / max(1.0f - spotCos, 1e-4f));
                att *= coneAtt * coneAtt;
            }
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

        float D = DistributionGGX(NdotH, roughnessSpecular);
        float G = GeometrySmith(NdotV, NdotL, roughnessSpecular);
        float3 F = FresnelSchlick(VdotH, F0);

        float3 spec = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-6f);
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metallic);
        float3 diffuse = kD * albedo;

        Lo += (diffuse + spec) * radiance * NdotL;
    }

    float ambient = dirLightCount > 0 ? 0.5f : 0.2f;
    float3 ambientDiffuse = ambient * albedo * (1.0f - metallic * 0.7f) * occulusion;
    float3 ambientF = FresnelSchlickRoughness(NdotV, F0, roughnessSpecular);
    float specAmbientStrength = lerp(0.02f, 0.25f, metallic) * lerp(1.0f, 0.6f, roughnessSpecular);
    float3 ambientSpec = ambient * ambientF * specAmbientStrength * occulusion;
    float3 color = ambientDiffuse + ambientSpec + Lo;

    float shadowF = ComputeShadow(posW);
    if (shadowF < 0.3f)
        shadowF = 0.0f;

    if (gShadowInvMapSize.x > 0.0f && gShadowInvMapSize.y > 0.0f)
    {
        float shadowValue = (1.0f - shadowF) * 0.2f;
        color -= shadowValue.xxx;
    }

    return float4(saturate(color), 1.0f);
}


