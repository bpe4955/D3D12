#include "ShaderIncludes.hlsli"


cbuffer PerFrame : register(b0)
{
    matrix view;
    matrix proj;

    //matrix shadowView;
    //matrix shadowProjection;
}
cbuffer perObject : register(b1)
{
    matrix world;
    matrix worldInvTranspose;
}

VertexToPixel main( VertexShaderInput input )
{
	// Set up output struct
    VertexToPixel output;
	
    matrix wvp = mul(proj, mul(view, world));
	
    output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));
    output.uv = input.uv;
    output.normal = mul((float3x3) worldInvTranspose, input.normal);
    output.worldPosition = mul(world, float4(input.localPosition, 1)).xyz;
    output.tangent = mul((float3x3) world, input.tangent);
    
	// Calculate where this vertex is from the light's point of view
    //matrix shadowWVP = mul(shadowProjection, mul(shadowView, world));
    //output.shadowMapPos = mul(shadowWVP, float4(input.localPosition, 1.0f));
	
    return output;
}