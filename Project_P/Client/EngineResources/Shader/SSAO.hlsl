// SSAO.hlsl - Screen Space Ambient Occlusion

#define SSAO_KERNEL_SIZE 16
#define SSAO_RADIUS 1.75f
#define SSAO_BIAS 0.0125f
#define SSAO_POWER 4.0f

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
cbuffer PerCustomValue : register(b5)
{
    float4x4 gInvViewProj;
};

cbuffer SSAOParams : register(b6)
{
    float4 gKernel[SSAO_KERNEL_SIZE];
    float2 gScreenSize;
    float2 gNoiseScale;
    float4x4 gCamView;
    float4x4 gCamProj;
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

float3 ReconstructWorldPos(float2 uv, float depth01)
{
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = uv.y * 2.0f - 1.0f;

    float4 clip = float4(ndc, depth01, 1.0f);
    float4 w = mul(clip, gInvViewProj);
    w.xyz /= w.w;
    return w.xyz;
}

float3 DecodeNormal(float3 enc01)
{
    return normalize(enc01 * 2.0f - 1.0f);
}

float3 WorldToViewPos(float3 worldPos)
{
    return mul(float4(worldPos, 1.0f), gCamView).xyz;
}

float3 WorldNormalToView(float3 worldNormal)
{
    return normalize(mul(float4(worldNormal, 0.0f), gCamView).xyz);
}

// Hash-based pseudo-random for noise (no noise texture needed)
float2 NoiseFromUV(float2 uv)
{
    float2 seed = uv * gScreenSize;
    float noiseX = frac(sin(dot(seed, float2(12.9898f, 78.233f))) * 43758.5453f);
    float noiseY = frac(sin(dot(seed, float2(93.9898f, 67.345f))) * 24634.6345f);
    return float2(noiseX, noiseY) * 2.0f - 1.0f;
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);

    if (depth01 >= 0.999999f)
        return float4(1, 1, 1, 1);

    float3 worldPos = ReconstructWorldPos(i.uv, depth01);
    float3 viewPos = WorldToViewPos(worldPos);
    float3 N = WorldNormalToView(DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz));

    // Build TBN from normal + noise
    float2 noise = NoiseFromUV(uvTex);
    float3 randomVec = normalize(float3(noise, 0.0f));

    float3 T = normalize(randomVec - N * dot(randomVec, N));
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float occlusion = 0.0f;

    [unroll]
    for (int s = 0; s < SSAO_KERNEL_SIZE; ++s)
    {
        float3 sampleOffset = mul(gKernel[s].xyz, TBN);
        float3 samplePos = viewPos + sampleOffset * SSAO_RADIUS;

        float4 clipPos = mul(float4(samplePos, 1.0f), gCamProj);
        if (clipPos.w <= 1e-6f)
            continue;
        clipPos.xyz /= clipPos.w;

        // NDC -> UV
        float2 sampleUV;
        sampleUV.x = clipPos.x * 0.5f + 0.5f;
        sampleUV.y = clipPos.y * 0.5f + 0.5f;
        sampleUV.y = 1.0f - sampleUV.y;

        if (sampleUV.x <= 0.0f || sampleUV.x >= 1.0f || sampleUV.y <= 0.0f || sampleUV.y >= 1.0f)
            continue;

        float sampleDepth = gDepth.SampleLevel(gSampler, sampleUV, 0);
        if (sampleDepth >= 0.999999f)
            continue;

        float3 sampleWorldPos = ReconstructWorldPos(float2(sampleUV.x, 1.0f - sampleUV.y), sampleDepth);
        float3 actualSamplePos = WorldToViewPos(sampleWorldPos);
        float dist = abs(actualSamplePos.z - viewPos.z);

        float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / max(dist, 1e-6f));
        occlusion += (actualSamplePos.z < samplePos.z - SSAO_BIAS ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / (float) SSAO_KERNEL_SIZE);
    occlusion = pow(saturate(occlusion), SSAO_POWER);

    return float4(occlusion, occlusion, occlusion, 1.0f);
}
