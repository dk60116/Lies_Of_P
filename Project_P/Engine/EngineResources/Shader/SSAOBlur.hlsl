// SSAOBlur.hlsl - Bilateral Blur for SSAO

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

cbuffer SSAOBlurParams : register(b5)
{
    float2 gTexelSize;
    float2 gBlurPadding;
};

Texture2D gSSAORaw : register(t0);
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

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    float centerDepth = gDepth.SampleLevel(gSampler, uvTex, 0);

    if (centerDepth >= 0.999999f)
        return float4(1, 1, 1, 1);

    float result = 0.0f;
    float totalWeight = 0.0f;

    static const int BLUR_SIZE = 2;
    float depthThreshold = 0.0005f;

    [unroll]
    for (int x = -BLUR_SIZE; x <= BLUR_SIZE; ++x)
    {
        [unroll]
        for (int y = -BLUR_SIZE; y <= BLUR_SIZE; ++y)
        {
            float2 offset = float2((float) x, (float) y) * gTexelSize;
            float2 sampleUV = uvTex + offset;

            float sampleDepth = gDepth.SampleLevel(gSampler, sampleUV, 0);
            float ssaoVal = gSSAORaw.Sample(gSampler, sampleUV).r;

            // Bilateral weight: reject samples with large depth difference
            float depthDiff = abs(centerDepth - sampleDepth);
            float depthWeight = (depthDiff < depthThreshold) ? 1.0f : 0.0f;

            // Spatial gaussian weight
            float dist = length(float2((float) x, (float) y));
            float spatialWeight = exp(-dist * dist / 8.0f);

            float w = depthWeight * spatialWeight;
            result += ssaoVal * w;
            totalWeight += w;
        }
    }

    result /= max(totalWeight, 1e-6f);

    return float4(result, result, result, 1.0f);
}
