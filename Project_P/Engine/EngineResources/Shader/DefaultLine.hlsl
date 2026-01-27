// ───────────── 상수 버퍼
cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    float3 gPos;
    float4x4 gView;
    float4x4 gProj;
    float cpadding;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor;
    uint boneCount;
    float3 padding; // 16바이트 정렬
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

// ───────────── 버텍스 입력
struct VSIn
{
    float3 posL : POSITION;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

// ───────────── 버텍스 출력
struct VSOut
{
    float4 posH : SV_POSITION;
};

// ───────────── 버텍스 셰이더
VSOut VSMain(VSIn v)
{
    VSOut o;

    float4 skinnedPos = float4(0, 0, 0, 0);
    if (boneCount)
    {
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            uint idx = v.boneIndices[i];
            float w = v.boneWeights[i];
            skinnedPos += mul(float4(v.posL, 1), gBones[idx]) * w;
        }
    }
    else
    {
        skinnedPos = float4(v.posL, 1);
    }

    // 월드, 뷰, 프로젝션 변환
    float4 posW = mul(skinnedPos, world);
    float4 posV = mul(posW, gView);
    o.posH = mul(posV, gProj);

    return o;
}

// ───────────── 픽셀 셰이더
float4 PSMain(VSOut input) : SV_TARGET
{
    // 단일 색상 출력
    return baseColor;
}