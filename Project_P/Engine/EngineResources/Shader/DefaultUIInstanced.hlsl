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
    float4 gInstanceFillParams[128];
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
    nointerpolation uint instanceID : TEXCOORD1;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float PI = 3.14159265359f;

float NormalizeAngle(float angle)
{
    angle = fmod(angle, PI * 2.0f);
    if (angle < 0.0f)
        angle += PI * 2.0f;
    return angle;
}

float ClockwiseAngleDelta(float startAngle, float targetAngle)
{
    return NormalizeAngle(startAngle - targetAngle);
}

float CounterClockwiseAngleDelta(float startAngle, float targetAngle)
{
    return NormalizeAngle(targetAngle - startAngle);
}

bool IsAngleWithinFill(float angle, float startAngle, float spanAngle, bool fillClockwise)
{
    const float epsilon = 1e-4f;
    float delta = fillClockwise
        ? ClockwiseAngleDelta(startAngle, angle)
        : CounterClockwiseAngleDelta(startAngle, angle);
    return delta <= spanAngle + epsilon;
}

bool Radial90Mask(float2 screenUV, float fillAmount, uint fillOrigin, bool fillClockwise)
{
    if (fillAmount <= 0.001f)
        return false;
    if (fillAmount >= 0.999f)
        return true;

    float2 localUV = 0.0f;

    if (fillOrigin == 0u)
        localUV = screenUV;
    else if (fillOrigin == 1u)
        localUV = float2(screenUV.x, 1.0f - screenUV.y);
    else if (fillOrigin == 2u)
        localUV = float2(1.0f - screenUV.x, 1.0f - screenUV.y);
    else
        localUV = float2(1.0f - screenUV.x, screenUV.y);

    if (localUV.x < 0.0f || localUV.y < 0.0f)
        return false;

    float angle = atan2(localUV.y, localUV.x);
    if (angle < 0.0f)
        angle += PI * 2.0f;

    float spanAngle = saturate(fillAmount) * (PI * 0.5f);
    float startAngle = fillClockwise ? (PI * 0.5f) : 0.0f;
    return IsAngleWithinFill(angle, startAngle, spanAngle, fillClockwise);
}

bool RadialCenterMask(float2 screenUV, float fillAmount, float startAngle, float totalSpanAngle, bool fillClockwise)
{
    if (fillAmount <= 0.001f)
        return false;
    if (fillAmount >= 0.999f)
        return true;

    float2 dir = screenUV - 0.5f;
    if (abs(dir.x) < 1e-5f && abs(dir.y) < 1e-5f)
        return true;

    float angle = NormalizeAngle(atan2(dir.y, dir.x));
    float spanAngle = saturate(fillAmount) * totalSpanAngle;
    return IsAngleWithinFill(angle, startAngle, spanAngle, fillClockwise);
}

bool ApplyFilledMask(float2 uv, float fillAmount, uint fillMethod, uint fillOrigin, bool fillClockwise)
{
    float2 screenUV = float2(uv.x, 1.0f - uv.y);

    if (fillMethod == 0u)
        return true;

    if (fillMethod == 1u)
        return (fillOrigin == 0u) ? (screenUV.x <= fillAmount) : (screenUV.x >= (1.0f - fillAmount));

    if (fillMethod == 2u)
        return (fillOrigin == 0u) ? (screenUV.y <= fillAmount) : (screenUV.y >= (1.0f - fillAmount));

    if (fillMethod == 3u)
        return Radial90Mask(screenUV, fillAmount, fillOrigin, fillClockwise);

    if (fillMethod == 4u)
        return RadialCenterMask(screenUV, fillAmount, ((float)fillOrigin * (PI * 0.5f)) - (PI * 0.5f), PI, fillClockwise);

    if (fillMethod == 5u)
        return RadialCenterMask(screenUV, fillAmount, ((float)fillOrigin * (PI * 0.5f)) - (PI * 0.5f), PI * 2.0f, fillClockwise);

    return true;
}

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
    output.instanceID = instanceID;

    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float4 resultColor = (useTexture != 0) ? gTexture.Sample(gSampler, input.uv) * gBaseColor : gBaseColor;

    if (resultColor.a < 0.01f)
        discard;

    float4 fillData = gImageParams;
    if (gInstanceCount > 0 && input.instanceID < gInstanceCount)
        fillData = gInstanceFillParams[input.instanceID];

    const float fillAmount = saturate(fillData.x);
    const uint fillMethod = (uint)round(fillData.y);
    const uint fillOrigin = (uint)round(fillData.z);
    const bool fillClockwise = (fillData.w >= 0.5f);

    if (!ApplyFilledMask(input.uv, fillAmount, fillMethod, fillOrigin, fillClockwise))
        discard;

    resultColor.rgb *= resultColor.a;

    return resultColor;
}
