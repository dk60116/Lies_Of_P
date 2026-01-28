cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 camPos;
    float cPadding;
    float4x4 view;
    float4x4 proj;
    float4x4 gInvViewProj;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint   useTexture;
    uint   boneCount;
    uint   useNormalMap;
    uint   _padMat0;    
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness;
    uint gObjectID;
    float2 gTiling;
    float2 gOffset;
};

Texture2D gTexture   : register(t0);
Texture2D gNormalMap : register(t1); 
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

struct PSOut
{
    float4 Albedo : SV_Target0;
    uint Object : SV_Target1;
    float4 Normal : SV_Target2;
    float4 Material : SV_Target3;
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

    T = normalize(T - N * dot(T, N));

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

PSOut PSMain(VSOut input)
{
    PSOut o;

    float2 uv = input.uv * gTiling + gOffset;

    float4 texColor = (useTexture != 0) ? gTexture.Sample(gSampler, uv) : float4(1,1,1,1);

    o.Albedo = saturate(baseColor * texColor);
    
    o.Object = gObjectID;

    float3 Nw = normalize(input.normalW);

    if (useNormalMap != 0)
    {
        float3 nTS = gNormalMap.Sample(gSampler, uv).xyz * 2.0f - 1.0f;

        float3 T = normalize(input.tangentW);
        float3 B = normalize(input.bitanW);
        float3 N = normalize(input.normalW);

        Nw = normalize(nTS.x * T + nTS.y * B + nTS.z * N);
    }

    o.Normal = float4(Nw * 0.5f + 0.5f, 1.0f);
    
    float f0 = 0.04f;
    o.Material = float4(0.f, gSmoothness, 0.f, 1.f);

    return o;
}
