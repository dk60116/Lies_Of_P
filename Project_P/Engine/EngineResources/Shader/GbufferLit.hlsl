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
    uint useTexture;
    uint useNormalMap;
    uint useORMMap;
    uint boneCount;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[512];
};

cbuffer PerInstance : register(b4)
{
    float4x4 gInstanceWorlds[128];
    uint gInstanceCount;
    float3 gInstancePadding;
};

cbuffer PerCustomValue : register(b10)
{
    float gOcculusion;
    float gRoughness;
    float gMetallic;
    uint gObjectID;
    float2 gTiling;
    float2 gOffset;
};

Texture2D gTexture   : register(t0);
Texture2D gNormalMap : register(t1); 
Texture2D gORMMap : register(t2);
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

VSOut VSMain(VSIn v, uint instanceID : SV_InstanceID)
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

            if (w > 0.0f && idx < 512)
            {
                float4x4 M = gBones[idx];
                skinnedPos += mul(float4(v.posL, 1.0f), M) * w;
                skinnedN += mul(v.normalL, (float3x3) M) * w;
                skinnedT += mul(v.tangentL, (float3x3) M) * w;
            }
        }
    }

    float4x4 worldMat = world;

    if (gInstanceCount > 0 && instanceID < gInstanceCount)
    {
        worldMat = gInstanceWorlds[instanceID];
    }

    float4 posW4 = mul(skinnedPos, worldMat);

    float3 N = normalize(mul(skinnedN, (float3x3) worldMat));
    float3 T = normalize(mul(skinnedT, (float3x3) worldMat));

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
        float3 nTS = gNormalMap.Sample(gSampler, uv).rgb * 2.0f - 1.0f;

        float3 T = normalize(input.tangentW);
        float3 B = normalize(input.bitanW);
        float3 N = normalize(input.normalW);

        Nw = normalize(nTS.x * T + nTS.y * B + nTS.z * N);
    }

    o.Normal = float4(Nw * 0.5f + 0.5f, 1.0f);
    
    float occ = gOcculusion;
    float rou = gRoughness;
    float met = gMetallic;
    
    if (useORMMap != 0)
    {
        float3 orm = gORMMap.Sample(gSampler, uv).rgb;
        
        occ = orm.r;
        rou = orm.g;
        met = orm.b;
    }
    
    o.Material = float4(occ, rou, met, 1.f);

    return o;
}
