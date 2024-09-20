#ifndef __GGP_LIGHTING__
#define __GGP_LIGHTING__

// === VARIABLES & DATA ============================================
#define MAX_LIGHTS 128

#define MAX_SPECULAR_EXPONENT 256.0f

#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

struct Light
{
    int Type;
    float3 Direction; // 16 bytes

    float Range;
    float3 Position; // 32 bytes

    float Intensity;
    float3 Color; // 48 bytes

    float SpotFalloff;
    float3 Padding; // 64 bytes
};

cbuffer PerFrame : register(b0)
{
    float3 cameraPosition;
    int lightCount;
    Light lights[MAX_LIGHTS];
    float4 ambient;
}

cbuffer PerMaterial : register(b1)
{
    float4 colorTint;
    float2 uvScale;
    float2 uvOffset;
}

// Textures
Texture2D SurfaceTexture : register(t0);
Texture2D NormalMap : register(t1);
Texture2D SpecularMap : register(t2);
Texture2D TextureMask : register(t3);
//TextureCube EnvironmentMap : register(t4);
SamplerState Sampler : register(s0);

// Constants
static const float F0_NON_METAL = 0.04f;
static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float HALF_PI = PI / 2.0f;
static const float QUARTER_PI = PI / 4.0f;

// === UTILITY FUNCTIONS ============================================
// Handle converting tangent-space normal map to world space normal
float3 NormalMapping(float2 uv, float3 normal, float3 tangent)
{
	// Grab the normal from the map
    float3 normalFromMap = NormalMap.Sample(Sampler, uv).rgb;
    
    float3 normalAdjusted = normalize(normalFromMap * 2 - 1);

	// Gather the required vectors for converting the normal
    float3 N = normal;
    float3 T = normalize(tangent - N * dot(tangent, N));
    float3 B = cross(T, N);

	// Create the 3x3 matrix to convert from TANGENT-SPACE normals to WORLD-SPACE normals
    float3x3 TBN = float3x3(T, B, N);

	// Adjust the normal from the map and simply use the results
    if (!any(normalFromMap))
        return normal;
    else
        return normalize(mul(normalAdjusted, TBN));
}

// === LIGHTING HELPERS ============================================
// Lambert diffuse BRDF
float Lambert(float3 normal, float3 lightDir)
{
    return saturate(dot(normal, -lightDir));
}

float Phong(float3 normal, float3 lightDir, float3 viewVector, float specularPower)
{
    float ret = 0.0f;
    if (specularPower != 0)
    {
    // Reflection of the Light coming off the surface
        float3 refl = reflect(lightDir, normal);
    // Get the angle between the view and reflection, saturate, raise to power
        ret = pow(saturate(dot(viewVector, refl)), specularPower);
    }
    return ret;
}

float SimpleFresnel(float3 n, float3 v, float f0)
{
	// Pre-calculations
    float NdotV = saturate(dot(n, v));

	// Final value
    return f0 + (1 - f0) * pow(1 - NdotV, 5);
}

float Attenuate(Light light, float3 worldPos)
{
    float dist = distance(light.Position, worldPos);
    float att = saturate(1.0f - (dist * dist / (light.Range * light.Range)));
    return att * att;
}

float ConeAttenuate(float3 dir, Light light, float3 worldPosition)
{
    float angleFromCenter = max(dot(normalize(worldPosition - light.Position), light.Direction), 0.0f);
    return pow(angleFromCenter, light.SpotFalloff);
}


// === LIGHTS =======================================================
float3 DirectionalLight(float3 normal, Light light, float3 viewVector, float specularPower, float3 surfaceColor, float specScale)
{
    float3 normLightDir = normalize(light.Direction);
    float diffuse = Lambert(normal, normLightDir);
    float specular = Phong(normal, normLightDir, viewVector, specularPower) * specScale * any(diffuse);
    
    return light.Intensity * (diffuse * surfaceColor + specular) * light.Color;
}

float3 PointLight(float3 normal, Light light, float3 viewVector, float specularPower, float3 worldPosition, float3 surfaceColor, float specScale)
{
    float3 normLightDir = normalize(worldPosition - light.Position);
    float diffuse = Lambert(normal, normLightDir);
    float specular = Phong(normal, normLightDir, viewVector, specularPower) * specScale * any(diffuse);
    
    return light.Intensity * (diffuse * surfaceColor + specular) * Attenuate(light, worldPosition) * light.Color;
}

float3 SpotLight(float3 normal, Light light, float3 viewVector, float specularPower, float3 worldPosition, float3 surfaceColor, float specScale)
{
    float3 normLightDir = normalize(light.Direction);
    float diffuse = Lambert(normal, normLightDir);
    float specular = Phong(normal, normLightDir, viewVector, specularPower) * specScale * any(diffuse);
    
    return light.Intensity * (diffuse * surfaceColor + specular) * Attenuate(light, worldPosition) * ConeAttenuate(normLightDir, light, worldPosition) * light.Color;
}

// assuming input values are not normalized
float4 totalLight(float3 normal, float3 worldPosition, float2 uv, float3 tangent)
{
    // normalize
    normal = normalize(normal);
    tangent = normalize(tangent);
    
    // Texturing 
    // (Includes checks if textures exist, which would make each pass take the same time regardless of SRVs)
    uv += uvOffset;
    uv *= uvScale;
    // Alpha Clipping
    float4 textureColor = SurfaceTexture.Sample(Sampler, uv);
    clip(textureColor.a - 0.1);
    float3 surfaceColor = pow(textureColor.rgb, 2.2f) * colorTint.rbg;
    float specScale = SpecularMap.Sample(Sampler, uv).r;
    if (specScale == 0)
        specScale = 1.0f;
    float3 maskSample = TextureMask.Sample(Sampler, uv).rgb;
    if (any(maskSample))
        surfaceColor *= maskSample;
    normal = NormalMapping(uv, normal, tangent);
    // Lighting
    float3 viewVector = normalize(cameraPosition - worldPosition);
    float3 totalLight = ambient.rbg * surfaceColor;
    float specularPower = (1.0f - colorTint.a) * MAX_SPECULAR_EXPONENT;
    
    // Light Calculations
    for (int i = 0; i < lightCount; i++)
    {
        switch (lights[i].Type)
        {
            case LIGHT_TYPE_DIRECTIONAL:
                totalLight += DirectionalLight(normal, lights[i], viewVector, specularPower, surfaceColor, specScale);
                break;
            case LIGHT_TYPE_POINT:
                totalLight += PointLight(normal, lights[i], viewVector, specularPower, worldPosition, surfaceColor, specScale);
                break;
            case LIGHT_TYPE_SPOT:
                totalLight += SpotLight(normal, lights[i], viewVector, specularPower, worldPosition, surfaceColor, specScale);
                break;
        }
    }
    float3 finalColor = pow(totalLight.rgb, 1.0f / 2.2f);
    
    // EnvironmentMap Reflections
    //if (hasEnvironmentMap)
    //{
    //    float3 reflectionVector = reflect(-viewVector, normal); // Need camera to pixel vector, so negate
    //    float3 reflectionColor = EnvironmentMap.Sample(Sampler, reflectionVector).rgb;
	//    // Interpolate between the surface color and reflection color using a Fresnel term
    //    finalColor = lerp(totalLight, reflectionColor, SimpleFresnel(normal, viewVector, F0_NON_METAL));
    //}
    return float4(finalColor, 1);
}

#endif