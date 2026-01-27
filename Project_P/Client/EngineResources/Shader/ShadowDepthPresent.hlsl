// ShadowDepthPresent.hlsl
// 목적: ShadowDepth SRV(t0)를 그레이스케일로 시각화
// 전제: ShadowDepth는 SRV 포맷이 DXGI_FORMAT_R32_FLOAT 로 생성되어 있음
// 바인딩: b0(world), b1(view/proj - 엔진 호환), t0(ShadowDepth), s0

cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 pos; // 사용 안 함(엔진 바인딩 호환)
    float _pad0;
    float4x4 view; // 사용 안 함(엔진 바인딩 호환)
    float4x4 proj; // 사용 안 함(엔진 바인딩 호환)
};

Texture2D<float> gShadowDepth : register(t0);
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
    float4 posW = mul(float4(v.posL, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.uv = v.uv;
    return o;
}

// 0~1 depth를 보기 좋게 매핑하는 옵션들
float RemapDepthForView(float d)
{
    // d가 0~1이 아닐 수도 있으니 안전하게 clamp
    d = saturate(d);

    // 1) 기본: 그대로 출력 (가까움=어두움, 멀어짐=밝아짐)
    // return d;

    // 2) Invert: 가까움=밝음, 멀어짐=어두움 (보통 더 직관적)
    return 1.0f - d;

    // 3) 대비 강화(원하면 사용):
    // float x = 1.0f - d;
    // return saturate(pow(x, 0.35f)); // 0.2~0.6 사이로 취향 조절
}

float4 PSMain(VSOut i) : SV_Target
{
    // 당신의 디버그 패스가 uv 뒤집힘을 쓰는 구조라면 아래처럼 맞추는 것이 안전합니다.
    // (DepthPresent/DeferredPresent에서 uvTex = float2(uv.x, 1-uv.y) 패턴을 이미 사용 중)
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    // ShadowDepth를 샘플링
    float d = gShadowDepth.SampleLevel(gSampler, uvTex, 0);

    // 보정/시각화 매핑
    float v = RemapDepthForView(d);

    return float4(v, v, v, 1.0f);
}
