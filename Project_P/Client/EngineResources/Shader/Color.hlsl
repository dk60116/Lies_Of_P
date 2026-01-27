cbuffer PerObject : register(b0)
{
    float4x4 gWorld;
}

cbuffer PerCamera : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
}

cbuffer PerMaterial : register(b2)
{
    float4 gBaseColor;
}

// ───── 버텍스 구조
struct VSIn
{
    float3 posL : POSITION;
};

struct VSOut
{
    float4 posH : SV_POSITION;
};

// ───── Vertex Shader
VSOut VSMain(VSIn input)
{
    VSOut output;

    float4 worldPos = mul(float4(input.posL, 1.0f), gWorld);
    float4 viewPos = mul(worldPos, gView);
    output.posH = mul(viewPos, gProj);

    return output;
}

// ───── Pixel Shader
float4 PSMain(VSOut input) : SV_TARGET
{
    return gBaseColor;
}

// ───── Technique
technique11 UnlitColor
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VSMain()));
        SetPixelShader(CompileShader(ps_5_0, PSMain()));
    }
}
