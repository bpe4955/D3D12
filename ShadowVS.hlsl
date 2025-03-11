#include "ShaderIncludes.hlsli"

// Constant Buffer for external (C++) data
cbuffer PerFrame : register(b0)
{
    matrix view;
    matrix proj;
}
cbuffer PerObject : register(b1)
{
    matrix world;
    matrix worldInvTranspose;
}

float4 main( VertexShaderInput input ) : SV_POSITION
{
    matrix wvp = mul(proj, mul(view, world));
    float4 screenPos = mul(wvp, float4(input.localPosition, 1.0f));
    return screenPos;
}