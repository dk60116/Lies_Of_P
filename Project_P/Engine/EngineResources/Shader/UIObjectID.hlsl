cbuffer PerObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer PerCamera : register(b1)
{
    float3 gPos;
    float4x4 gView;
    float4x4 gProj;
    float cpadding;
}

cbuffer PerMaterial : register(b2)
{
    float4 gBaseColor;
    uint useTexture;
    uint boneCount;
    float2 padding;
}

cbuffer PerCustomValue : register(b10)
{
    uint gObjectID;
    float3 customPadding;
}

struct VSIn
{
    float3 posL : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 posH : SV_POSITION;
};

VSOut VSMain(VSIn input)
{
    VSOut output;

    float4 worldPos = mul(float4(input.posL, 1.0f), gWorld);
    float4 viewPos = mul(worldPos, gView);
    output.posH = mul(viewPos, gProj);

    return output;
}

uint PSMain(VSOut input) : SV_Target0
{
    return gObjectID;
}
