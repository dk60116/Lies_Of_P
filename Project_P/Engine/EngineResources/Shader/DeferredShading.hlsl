// DeferredLighting_DiffuseOnly.hlsl

// 라이트 정의
#define MAX_LIGHTS 64

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

cbuffer PerObject : register(b0)
{
    float4x4 world; // fullscreen quad용 (그대로 유지)
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

Texture2D gNormal : register(t0);
Texture2D<float> gDepth : register(t1);
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
    // uv는 이미 y-flip 된 상태라고 가정 (uv.y = 1-uv.y 적용 후)
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = uv.y * 2.0f - 1.0f;

    // D3D NDC z는 0..1 이므로 depth01 그대로 사용
    float4 clip = float4(ndc, depth01, 1.0f);

    float4 world = mul(clip, gInvViewProj);
    world.xyz /= world.w;

    return world.xyz;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvScreen = i.uv;
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float3 N = DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz);

    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);
    if (depth01 >= 0.999999f)
        return float4(1.f, 1.f, 1.f, 1);

    float2 ndc = float2(uvScreen.x * 2 - 1, uvScreen.y * 2 - 1);

    float4 clip = float4(ndc, depth01, 1);
    float4 wpos4 = mul(clip, gInvViewProj);
    float3 posW = wpos4.xyz / wpos4.w;

    float3 V = normalize(camPos - posW);

    float3 diffuseSum = 0;
    float3 ambientSum = 0;
    float3 specularSum = 0;

    int lightCount = (int) gLight[0][3][3];

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
        float attenK = gLight[li][3][1];
        float ambientK = gLight[li][2][3];

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
        diffuseSum += lightCol * (NdotL * intensity * att);
    }

    float3 globalAmbient = 0.2f;

    // "알베도는 빼고 명암만" 이므로 조명 결과만 출력
    float3 lit = saturate(globalAmbient + ambientSum + diffuseSum + specularSum);
    return float4(lit, 1);
}