// DeferredLighting_DiffuseOnly.hlsl

cbuffer PerObject : register(b0)
{
    float4x4 world; // fullscreen quad용 (그대로 유지)
};

cbuffer PerCamera : register(b1)
{
    float3 camPos; // 사용 안 함(바인딩 호환)
    float4x4 view; // 사용 안 함
    float4x4 proj; // 사용 안 함
    float cpadding;
};

#pragma pack_matrix(row_major)
cbuffer PerLight : register(b4)
{
    float4x4 gLight[64];
};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
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
    // view/proj를 안 쓰려면 world에서 이미 clip-space로 만들거나,
    // 기존 Present처럼 view/proj를 Identity/Ortho로 넣어도 됩니다.
    // 여기서는 기존 Camera::RenderRTDebugDisplay 흐름 그대로(view/proj 바인딩) 사용을 전제로 둡니다.
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.uv = v.uv;
    return o;
}

float3 DecodeNormal(float3 enc01)
{
    return normalize(enc01 * 2.0f - 1.0f);
}

float4 PSMain(VSOut i) : SV_Target
{
    float2 uv = i.uv;

    float3 albedo = gAlbedo.Sample(gSampler, uv).rgb;
    float3 N = DecodeNormal(gNormal.Sample(gSampler, uv).xyz);

    // gLight[0]을 directional로 사용(당신의 포맷에 맞춤)
    uint lightType = (uint) gLight[0][3][0];
    float3 lightDir = float3(gLight[0][1][0], gLight[0][1][1], gLight[0][1][2]);
    float3 lightColor = float3(gLight[0][2][0], gLight[0][2][1], gLight[0][2][2]);
    float intensity = gLight[0][1][3];
    float ambientK = gLight[0][2][3];

    // 방향광만 처리
    float3 L = normalize(-lightDir);
    float ndotl = saturate(dot(N, L));

    float3 ambient = lightColor * ambientK;
    float3 diffuse = lightColor * ndotl * intensity;

    float3 lit = albedo * saturate(ambient + diffuse);
    return float4(saturate(lit), 1);
}
