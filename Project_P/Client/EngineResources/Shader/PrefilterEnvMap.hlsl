// PrefilterEnvMap.hlsl
// Renders pre-filtered specular environment cubemap (128x128, 5 mip levels)
// Importance sampling GGX with roughness per mip level

#define PI 3.14159265359
#define SAMPLE_COUNT 1024u

cbuffer BakeCB : register(b0)
{
    float4x4 gInvFaceViewProj;
    float    gRoughness;
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

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 N = normalize(i.dir);
    float3 R = N;
    float3 V = R;

    float  totalWeight = 0.0;
    float3 prefilteredColor = float3(0, 0, 0);

    float roughness = max(gRoughness, 0.001);

    for (uint s = 0u; s < SAMPLE_COUNT; ++s)
    {
        float2 Xi = Hammersley(s, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += gEnvMap.SampleLevel(gSampler, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= max(totalWeight, 0.001);
    return float4(prefilteredColor, 1.0);
}
