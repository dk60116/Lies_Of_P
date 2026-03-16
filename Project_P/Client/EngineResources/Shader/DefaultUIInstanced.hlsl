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

cbuffer PerFillAmount : register(b3)
{
    float4 gImageParams;
}

cbuffer PerInstance : register(b4)
{
    float4x4 gInstanceWorlds[128];
    uint gInstanceCount;
    float3 gInstancePadding;
}

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

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

VSOut VSMain(VSIn input, uint instanceID : SV_InstanceID)
{
    VSOut output;

    float4x4 worldMat = gWorld;
    if (gInstanceCount > 0 && instanceID < gInstanceCount)
        worldMat = gInstanceWorlds[instanceID];

    float4 worldPos = mul(float4(input.posL, 1.0f), worldMat);
    float4 viewPos = mul(worldPos, gView);
    output.posH = mul(viewPos, gProj);
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float4 resultColor = (useTexture != 0) ? gTexture.Sample(gSampler, input.uv) * gBaseColor : gBaseColor;

    if (resultColor.a < 0.01f)
        discard;

    const float fillAmount = saturate(gImageParams.x);
    const uint fillMethod = (uint)round(gImageParams.y);

    if (fillMethod == 1)
    {
        if (input.uv.x >= fillAmount)
            discard;
    }
    else if (fillMethod == 2)
    {
        if ((1.0f - input.uv.y) >= fillAmount)
            discard;
    }

    return resultColor;
}
