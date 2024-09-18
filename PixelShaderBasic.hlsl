#include "ShaderIncludes.hlsli"
#include "LightingBasic.hlsli"

float4 main(VertexToPixel input) : SV_TARGET
{
    return totalLight(input.normal, input.worldPosition, input.uv, input.tangent);
}