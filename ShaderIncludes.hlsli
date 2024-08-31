#ifndef __GGP_SHADERINCLUDE__
#define __GGP_SHADERINCLUDE__

struct VertexShaderInput
{
    float3 localPosition : POSITION; // XYZ position
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VertexToPixel
{
    float4 screenPosition : SV_POSITION;
};

struct VertexToPixel_Sky
{
    float4 screenPosition : SV_POSITION;
    float3 sampleDir : DIRECTION;
};

#endif