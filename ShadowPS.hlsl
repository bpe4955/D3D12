#include "ShaderIncludes.hlsli"


float main(float4 input : SV_POSITION) : SV_DEPTH
{
    if(input.w == 1)
        return input.z;
    
    return 1 - (input.z / input.w);
}