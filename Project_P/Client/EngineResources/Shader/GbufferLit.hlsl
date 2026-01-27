// GBufferLit.hlsl (GBuffer + NormalMap, 스킨 포함)

cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float4x4 view;
    float4x4 proj;
    float4x4 gInvViewProj;
    float cPadding;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint   useTexture;
    uint   boneCount;
    uint   useNormalMap;
    uint   _padMat0;     // 패딩(권장)
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness;
    float2 gTiling;
    float2 gOffset;
};

Texture2D gTexture   : register(t0); // Albedo
Texture2D gNormalMap : register(t1); // NormalMap (Linear SRV 권장)
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangentL : TANGENT;
    uint4  boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOut
{
    float4 posH     : SV_POSITION;
    float3 posW     : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float3 normalW  : TEXCOORD2;
    float3 tangentW : TEXCOORD3;
    float3 bitanW   : TEXCOORD4;
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    float4 skinnedPos = float4(v.posL, 1.0f);
    float3 skinnedN   = v.normalL;
    float3 skinnedT   = v.tangentL;

    if (boneCount != 0)
    {
        skinnedPos = 0;
        skinnedN   = 0;
        skinnedT   = 0;

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint  idx = v.boneIndices[i];

            if (w > 0.0f && idx < 128)
            {
                float4x4 M = gBones[idx];
                skinnedPos += mul(float4(v.posL, 1.0f), M) * w;
                skinnedN += mul(v.normalL, (float3x3) M) * w;
                skinnedT += mul(v.tangentL, (float3x3) M) * w;
            }
        }
    }

    float4 posW4 = mul(skinnedPos, world);

    float3 N = normalize(mul(skinnedN, (float3x3) world));
    float3 T = normalize(mul(skinnedT, (float3x3) world));

    // Gram-Schmidt로 T를 N에 직교화(노말맵 품질 안정화)
    T = normalize(T - N * dot(T, N));

    // handedness(tangent.w)가 없으니 우선 cross(N,T) 사용
    float3 B = normalize(cross(N, T));

    float4 posV = mul(posW4, view);
    o.posH = mul(posV, proj);

    o.posW = posW4.xyz;
    o.uv = v.uv;
    o.normalW = N;
    o.tangentW = T;
    o.bitanW = B;

    return o;
}

struct PSOut
{
    float4 Albedo   : SV_Target0;
    float4 Normal   : SV_Target1;
    float4 Specular : SV_Target2;
};

PSOut PSMain(VSOut input)
{
    PSOut o;

    float2 uv = input.uv * gTiling + gOffset;

    float4 texColor = (useTexture != 0) ? gTexture.Sample(gSampler, uv) : float4(1,1,1,1);

    // Albedo
    o.Albedo = saturate(baseColor * texColor);

    // World Normal
    float3 Nw = normalize(input.normalW);

    if (useNormalMap != 0)
    {
        float3 nTS = gNormalMap.Sample(gSampler, uv).xyz * 2.0f - 1.0f;

        // 만약 노말맵이 OpenGL(Y+) 기준이라면 아래 한 줄을 켜세요(초록 채널 뒤집기)
        // nTS.y = -nTS.y;

        float3 T = normalize(input.tangentW);
        float3 B = normalize(input.bitanW);
        float3 N = normalize(input.normalW);

        Nw = normalize(nTS.x * T + nTS.y * B + nTS.z * N);
    }

    // Encode to 0~1
    o.Normal = float4(Nw * 0.5f + 0.5f, 1.0f);
    
    float f0 = 0.04f;
    o.Specular = float4(0.f, gSmoothness, 0.f, 1.f);

    return o;
}
