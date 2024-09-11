#include "ShaderIncludes.hlsli"

cbuffer DataFromCPU : register(b0)
{
    matrix view;
    matrix proj;
}

VertexToPixel_Sky main( VertexShaderInput input )
{
    VertexToPixel_Sky output;
    
    matrix viewNoTranslation = view;
    viewNoTranslation._14 = 0;
    viewNoTranslation._24 = 0;
    viewNoTranslation._34 = 0;
    
    matrix vp = mul(proj, viewNoTranslation);
    output.screenPosition = mul(vp, float4(input.localPosition, 1.0f));
    output.screenPosition.z = output.screenPosition.w;
    
    output.sampleDir = input.localPosition;
    
	return output;
}