// IrradianceConvolution.hlsl
// Renders diffuse irradiance cubemap (32x32 per face)
// Cosine-weighted hemisphere integration of source environment cubemap

#define PI 3.14159265359
#define SAMPLE_DELTA 0.025

cbuffer BakeCB : register(b0)
{
    float4x4 gInvFaceViewProj;
    float    gRoughness;   // unused
    float3   gPad;
};

TextureCube  gEnvMap  : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION;
    float2 uv   : TEXCOORD0;
};

struct VSOut
{
    float4 posH : SV_POSITION;
    float3 dir  : TEXCOORD0;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    float2 ndc = v.uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float4 worldDir = mul(float4(ndc, 1.0, 1.0), gInvFaceViewProj);
    o.dir = normalize(worldDir.xyz);
    o.posH = float4(ndc, 0.5, 1.0);
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 N = normalize(i.dir);
    float3 irradiance = float3(0, 0, 0);

    float3 up    = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float sampleCount = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += SAMPLE_DELTA)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += SAMPLE_DELTA)
        {
            float cosT = cos(theta);
            float sinT = sin(theta);

            float3 tangentSample = float3(
                sinT * cos(phi),
                sinT * sin(phi),
                cosT);

            float3 sampleVec = tangentSample.x * right
                             + tangentSample.y * up
                             + tangentSample.z * N;

            irradiance += gEnvMap.SampleLevel(gSampler, sampleVec, 0).rgb * cosT * sinT;
            sampleCount += 1.0;
        }
    }

    irradiance = PI * irradiance / max(sampleCount, 1.0);
    return float4(irradiance, 1.0);
}
