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
    float _padShadow0;
};

Texture2D<float> gSceneDepth : register(t0); 
Texture2D<float> gShadowDepth : register(t1);
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

float SampleShadowPCF(float2 uv, float receiverDepth)
{
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

float4 PSMain(VSOut i) : SV_Target
{
    float2 uv = i.uv;
    uv.y = 1.0f - uv.y;

    float sceneDepth = gSceneDepth.SampleLevel(gSampler, uv, 0);
    
    float d = gShadowDepth.SampleLevel(gSampler, uv, 0);

    float v = saturate((1.0f - d));
    return float4(v, v, v, 1);
    
    if (sceneDepth >= 0.999999f)
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
        return float4(1, 1, 1, 1);

    float lit = SampleShadowPCF(uvL, depthL);
    return float4(lit, lit, lit, 1.0f);
}
