// ShadowMask.hlsl

cbuffer PerObject : register(b0)
{
    float4x4 world; // fullscreen quad용 (사용 안 해도 엔진 바인딩 호환)
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
    float4x4 gShadowViewProj; // World -> LightClip
    float2 gShadowInvMapSize; // 1.0 / shadowMapSize
    float gShadowBias; // e.g. 0.001 ~ 0.01
    float _padShadow0;
};

Texture2D<float> gSceneDepth : register(t0); // RTType::Depth SRV (R24_UNORM_X8)
Texture2D<float> gShadowDepth : register(t1); // RTType::ShadowDepth SRV (R32_FLOAT 등)
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
    uv.y = 1.0f - uv.y; // 이건 지금 엔진의 RT 샘플링 규칙에 맞춰 유지

    float sceneDepth = gSceneDepth.SampleLevel(gSampler, uv, 0);
    
    if (sceneDepth >= 0.999999f)
        return float4(1, 1, 1, 1);

    float2 ndcXY = float2(
    uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f);
    float4 posH = float4(ndcXY, sceneDepth, 1.0f);

    float4 posW4 = mul(posH, gInvViewProj);
    posW4.xyz /= posW4.w;

    float4 posL = mul(float4(posW4.xyz, 1.0f), gShadowViewProj);
    float3 ndcL = posL.xyz / posL.w;

    // ★ 여기: Y 뒤집기
    float2 uvL = float2(ndcL.x * 0.5f + 0.5f, -ndcL.y * 0.5f + 0.5f);

    float depthL = ndcL.z; // (만약 -1..1이면: depthL = depthL*0.5+0.5)

    if (uvL.x < 0 || uvL.x > 1 || uvL.y < 0 || uvL.y > 1 || depthL < 0 || depthL > 1)
        return float4(1, 1, 1, 1);

    float lit = SampleShadowPCF(uvL, depthL);
    return float4(lit, lit, lit, 1.0f);
}
