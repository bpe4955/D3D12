#include "ShaderIncludes.hlsli"

/// <summary>
/// Layout of our constant buffer
/// </summary>
cbuffer DataFromCPU : register(b0)
{
    matrix world;
    matrix worldInvTranspose;
    matrix view;
    matrix proj;

    //matrix shadowView;
    //matrix shadowProjection;
}

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// 
// - Input is exactly one vertex worth of data (defined by a struct)
// - Output is a single struct of data to pass down the pipeline
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
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