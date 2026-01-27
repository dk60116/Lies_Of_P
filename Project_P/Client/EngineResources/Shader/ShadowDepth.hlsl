// ShadowDepth.hlsl (Depth-only + 스킨 포함, GBufferLit와 유사 구조)

cbuffer PerObject : register(b0)
{
    float4x4 world;
};

cbuffer PerCamera : register(b1)
{
    // 이 pass에서의 "camera"는 라이트 카메라(view/proj)
    float3 camPos; // 미사용(바인딩 호환)
    float4x4 view;
    float4x4 proj;

    // GBufferLit에 들어있던 gInvViewProj까지 동일하게 두되,
    // ShadowDepth에서는 보통 미사용. (바인딩 호환용)
    float4x4 gInvViewProj;

    float cPadding;
};

cbuffer PerMaterial : register(b2)
{
    float4 baseColor; // 미사용(바인딩 호환)
    uint useTexture; // 알파테스트 시 사용 가능
    uint boneCount;
    uint useNormalMap; // 미사용(바인딩 호환)
    uint _padMat0;
};

cbuffer PerBones : register(b3)
{
    float4x4 gBones[128];
};

cbuffer PerCustomValue : register(b10)
{
    float gSmoothness; // 미사용(바인딩 호환)
    float2 gTiling;
    float2 gOffset;
};

// (선택) 알파테스트가 필요하면 t0/s0 유지
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL; // 미사용
    float2 uv : TEXCOORD0; // 알파테스트 대비
    float3 tangentL : TANGENT; // 미사용
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOut
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0; // (선택) 알파테스트 대비
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    // ----- 스킨 포지션 (GBufferLit과 동일 로직) -----
    float4 skinnedPos = float4(v.posL, 1.0f);

    if (boneCount != 0)
    {
        skinnedPos = 0;

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float w = v.boneWeights[i];
            uint idx = v.boneIndices[i];

            if (w > 0.0f && idx < 128)
            {
                float4x4 M = gBones[idx];
                skinnedPos += mul(float4(v.posL, 1.0f), M) * w;
            }
        }
    }

    // ----- 라이트 View/Proj로 클립 좌표 출력 -----
    float4 posW = mul(skinnedPos, world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);

    o.uv = v.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return 0;
}
