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
cbuffer InvViewProjCB : register(b5)
{
    float4x4 gInvViewProj;
};

cbuffer ShadowCB : register(b6)
{
    float4x4 gShadowViewProj;
    float2 gShadowInvMapSize;
    float gShadowBias;
    float gLightSize;
    float3 gShadowLightDir;
    float _padShadow1;
};

Texture2D<float> gSceneDepth : register(t0);
Texture2D gNormal : register(t1);
Texture2D<float> gShadowDepth : register(t2);
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

static const float2 gPoissonDisk[16] =
{
    float2(-0.94201624f, -0.39906216f),
    float2( 0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2( 0.34495938f,  0.29387760f),
    float2(-0.91588581f,  0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f,  0.27676845f),
    float2( 0.97484398f,  0.75648379f),
    float2( 0.44323325f, -0.97511554f),
    float2( 0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2( 0.79197514f,  0.19090188f),
    float2(-0.24188840f,  0.99706507f),
    float2(-0.81409955f,  0.91437590f),
    float2( 0.19984126f,  0.78641367f),
    float2( 0.14383161f, -0.14100790f)
};

float FindBlockerDistance(float2 uv, float receiverDepth, float searchWidth)
{
    float blockerSum = 0.0f;
    int numBlockers = 0;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 offset = gPoissonDisk[i] * searchWidth;
        float sd = gShadowDepth.SampleLevel(gSampler, uv + offset, 0);
        if (sd + gShadowBias < receiverDepth)
        {
            blockerSum += sd;
            ++numBlockers;
        }
    }

    if (numBlockers == 0)
        return -1.0f;

    return blockerSum / (float)numBlockers;
}

float PCF_Filter(float2 uv, float receiverDepth, float filterRadius)
{
    float sum = 0.0f;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 offset = gPoissonDisk[i] * filterRadius;
        float sd = gShadowDepth.SampleLevel(gSampler, uv + offset, 0);
        sum += (sd + gShadowBias < receiverDepth) ? 0.0f : 1.0f;
    }

    return sum / 16.0f;
}

float PCSS(float2 uv, float receiverDepth)
{
    float searchWidth = gLightSize * gShadowInvMapSize.x;
    float avgBlockerDepth = FindBlockerDistance(uv, receiverDepth, searchWidth);

    if (avgBlockerDepth < 0.0f)
        return 1.0f;

    float penumbraRatio = (receiverDepth - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = penumbraRatio * gLightSize * gShadowInvMapSize.x;
    filterRadius = clamp(filterRadius, gShadowInvMapSize.x, gShadowInvMapSize.x * 64.0f);

    return PCF_Filter(uv, receiverDepth, filterRadius);
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uv = i.uv;
    uv.y = 1.0f - uv.y;

    float sceneDepth = gSceneDepth.SampleLevel(gSampler, uv, 0);

    if (sceneDepth >= 0.999999f)
        return float4(1, 1, 1, 1);

    float3 lightDir = -gShadowLightDir;
    float lightLenSq = dot(lightDir, lightDir);
    if (lightLenSq <= 1e-6f)
        return float4(1, 1, 1, 1);

    lightDir *= rsqrt(lightLenSq);

    float3 N = DecodeNormal(gNormal.Sample(gSampler, uv).xyz);
    if (dot(N, lightDir) <= 1e-6f)
        return float4(1, 1, 1, 1);

    float2 ndcXY = float2(
    uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4 posH = float4(ndcXY, sceneDepth, 1.0f);

    float4 posW4 = mul(posH, gInvViewProj);
    posW4.xyz /= posW4.w;

    float4 posL = mul(float4(posW4.xyz, 1.0f), gShadowViewProj);
    float3 ndcL = posL.xyz / posL.w;

    float2 uvL = float2(ndcL.x * 0.5f + 0.5f, -ndcL.y * 0.5f + 0.5f);

    float depthL = ndcL.z;

    if (uvL.x < 0 || uvL.x > 1 || uvL.y < 0 || uvL.y > 1 || depthL < 0 || depthL > 1)
        return float4(1.f, 1.f, 1.f, 1.f);

    float lit = PCSS(uvL, depthL);
    
    if (lit > 0.8f)
        lit = 1.f;
    
    return float4(lit, lit, lit, 1.0f);
}
