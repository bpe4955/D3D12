// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 colorTint : COLOR;
};

Texture2D       Texture : register(t0);
SamplerState    Sampler : register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	// Return the texture sample
    return Texture.Sample(Sampler, input.uv) * input.colorTint;
	//return float4(1.0f, 1.0f, 1.0f, 1.0f);
}