#include "ShaderIncludes.hlsli"

// Note the allignment
cbuffer ExternalData : register(b0)
{
    float2 uvScale;
    float2 uvOffset;
    float3 cameraPosition;
    //int lightCount;
    //Light lights[MAX_LIGHTS];
}


// Texture related
Texture2D AlbedoTexture : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D MetalMap : register(t3);
SamplerState BasicSampler : register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
    
    // Scale and offset uv as necessary
    input.uv = input.uv * uvScale + uvOffset;
    
	// Normal mapping
    //input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);
    
    // Surface color with gamma correction
    float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
    surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);
    
    // Sample the other maps
    float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
    float metal = MetalMap.Sample(BasicSampler, input.uv).r;
    
    return surfaceColor;
}