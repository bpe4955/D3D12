#include "ShaderIncludes.hlsli"


float main(float4 input : SV_POSITION) : SV_DEPTH
{
    return 1 - (input.z / input.w);
}