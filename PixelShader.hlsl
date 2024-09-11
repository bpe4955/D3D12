#include "ShaderIncludes.hlsli"
#include "Lighting.hlsli"



float4 main(VertexToPixel input) : SV_TARGET
{
    // Sample the other maps
    return totalLight(input.normal, input.worldPosition, input.uv, input.tangent);
}