// DeferredSpecular_ForwardMatch.hlsl
// 목적: Forward 셰이더의 Specular(Reflect-Phong) 계산식을 Deferred에서 동일하게 재현
// t1: Normal, t2: Depth, t3: Material(=SpecParams: .g = smoothness/strength), b5: gInvViewProj

#define MAX_LIGHTS 64
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1

cbuffer PerObject : register(b0)
{
    float4x4 world; // fullscreen quad world (pixel-space)
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float cpadding0;
    float4x4 view;
    float4x4 proj;
};

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[MAX_LIGHTS];
};

#pragma pack_matrix(row_major)
cbuffer PerCustomValue : register(b5)
{
    float4x4 gInvViewProj;
};

Texture2D gNormal : register(t0);
Texture2D<float> gDepth : register(t1);
Texture2D gSpecParams : register(t2); 
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

float3 ReconstructWorldPos(float2 uvScreen, float depth01)
{
    float2 ndc = float2(uvScreen.x * 2.0f - 1.0f, uvScreen.y * 2.0f - 1.0f);
    float4 clip = float4(ndc, depth01, 1.0f);
    float4 w = mul(clip, gInvViewProj); // row_major + clip(행벡터) * M 형태 유지
    return w.xyz / max(w.w, 1e-6f);
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uvScreen = i.uv;
    float2 uvTex = float2(i.uv.x, 1.0f - i.uv.y);

    // Depth
    float depth01 = gDepth.SampleLevel(gSampler, uvTex, 0);

    // Sky / background
    if (depth01 >= 0.999999f)
        return float4(0, 0, 0, 1);

    // Normal / WorldPos
    float3 N = DecodeNormal(gNormal.Sample(gSampler, uvTex).xyz);
    float3 posW = ReconstructWorldPos(uvScreen, depth01);

    // Forward의 gSmoothness에 해당하는 strength를 Material RT에서 가져옴
    float4 sp = gSpecParams.Sample(gSampler, uvTex);
    float specularStrength = sp.g;

    // 카메라 벡터 (Forward: V = normalize(pos - input.posW))
    float3 V = normalize(camPos - posW);

    float3 specSum = 0;

    // LightCount: gLight[0][3][3] 규약 유지
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
        float attenuationK = gLight[li][3][1];

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
            float dist = length(toL);

            // Forward와 동일한 형태로 early-out
            if (range <= 1e-6f || dist >= range)
                continue;

            L = toL / max(dist, 1e-6f);

            // Forward와 동일: saturate(1 - dist/range) * attenuationK
            att = saturate(1.0f - dist / range) * attenuationK;
        }
        else
        {
            continue;
        }

        // Forward Specular (Reflect-Phong):
        // R = reflect(-L, N)
        // fSpecular = pow(saturate(dot(R, V)), 50)
        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        float fSpecular = pow(RdotV, 50.0f);

        float3 specular = lightCol * (fSpecular * specularStrength * intensity * att);
        specSum += specular;
    }

    // spec only 출력
    specSum = saturate(specSum);
    return float4(specSum, 1);
}
