#ifndef __GGP_SHADERINCLUDE__
#define __GGP_SHADERINCLUDE__

#define MAX_SHADOWLIGHTS 5

struct VertexShaderInput
{
    float3 localPosition    : POSITION; // XYZ position
    float2 uv               : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
};

struct VertexToPixel
{
    float4 screenPosition   : SV_POSITION;
    float2 uv               : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 worldPosition    : POSITION;
    float4 shadowMapPos[MAX_SHADOWLIGHTS] : SHADOW_POSITION;
    int    shadowlightCount : SHADOW_COUNT;
};

struct VertexToPixel_Sky
{
    float4 screenPosition   : SV_POSITION;
    float3 sampleDir        : DIRECTION;
};

#endif