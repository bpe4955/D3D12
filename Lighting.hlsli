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

// Texture related
Texture2D AlbedoTexture : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D MetalMap : register(t3);
SamplerState Sampler : register(s0);

// PBR Constants:
// The fresnel value for non-metals (dielectrics)
// Page 9: "F0 of nonmetals is now a constant 0.04"
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// Also slide 65 of http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
static const float F0_NON_METAL = 0.04f;
// Need a minimum roughness for when spec distribution function denominator goes to zero
static const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal
static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float HALF_PI = PI / 2.0f;
static const float QUARTER_PI = PI / 4.0f;

// === UTILITY FUNCTIONS ============================================
// Handle converting tangent-space normal map to world space normal
float3 NormalMapping(float2 uv, float3 normal, float3 tangent)
{
	// Grab the normal from the map
    float3 normalFromMap = normalize(NormalMap.Sample(Sampler, uv).rgb * 2 - 1);

	// Gather the required vectors for converting the normal
    float3 N = normal;
    float3 T = normalize(tangent - N * dot(tangent, N));
    float3 B = cross(T, N);

	// Create the 3x3 matrix to convert from TANGENT-SPACE normals to WORLD-SPACE normals
    float3x3 TBN = float3x3(T, B, N);

	// Adjust the normal from the map and simply use the results
    return normalize(mul(normalFromMap, TBN));
}

// === PHYSICALLY BASED LIGHTING HELPERS ====================================

// Lambert diffuse BRDF - Same as the basic lighting!
float Lambert(float3 normal, float3 dirToLight)
{
    return saturate(dot(normal, dirToLight));
}

// GGX (Trowbridge-Reitz)
//
// a - Roughness
// h - Half vector
// n - Normal
// 
// D(h, n) = a^2 / pi * ((n dot h)^2 * (a^2 - 1) + 1)^2
float SpecDistribution(float3 n, float3 h, float roughness)
{
	// Pre-calculations
    float NdotH = saturate(dot(n, h));
    float NdotH2 = NdotH * NdotH;
    float a = roughness * roughness;
    float a2 = max(a * a, MIN_ROUGHNESS); // Applied after remap!

	// ((n dot h)^2 * (a^2 - 1) + 1)
    float denomToSquare = NdotH2 * (a2 - 1) + 1;
	// Can go to zero if roughness is 0 and NdotH is 1

	// Final value
    return a2 / (PI * denomToSquare * denomToSquare);
}
// Fresnel term - Schlick approx.
// 
// v - View vector
// h - Half vector
// f0 - Value when l = n (full specular color)
//
// F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5
float3 Fresnel(float3 v, float3 h, float3 f0)
{
	// Pre-calculations
    float VdotH = saturate(dot(v, h));

	// Final value
    return f0 + (1 - f0) * pow(1 - VdotH, 5);
}
// Geometric Shadowing - Schlick-GGX (based on Schlick-Beckmann)
// - k is remapped to a / 2, roughness remapped to (r+1)/2
//
// n - Normal
// v - View vector
//
// G(l,v,h)
float GeometricShadowing(float3 n, float3 v, float3 h, float roughness)
{
	// End result of remapping:
    float k = pow(roughness + 1, 2) / 8.0f;
    float NdotV = saturate(dot(n, v));

	// Final value
    return NdotV / (NdotV * (1 - k) + k);
}
// Microfacet BRDF (Specular)
//
// f(l,v) = D(h)F(v,h)G(l,v,h) / 4(n dot l)(n dot v)
// - part of the denominator are canceled out by numerator (see below)
//
// D() - Spec Dist - Trowbridge-Reitz (GGX)
// F() - Fresnel - Schlick approx
// G() - Geometric Shadowing - Schlick-GGX
float3 MicrofacetBRDF(float3 n, float3 l, float3 v, float roughness, float3 specColor)
{
	// Other vectors
    float3 h = normalize(v + l);

	// Grab various functions
    float D = SpecDistribution(n, h, roughness);
    float3 F = Fresnel(v, h, specColor); // This ranges from F0_NON_METAL to actual specColor based on metalness
    float G = GeometricShadowing(n, v, h, roughness) * GeometricShadowing(n, l, h, roughness);

	// Final formula
	// Denominator dot products partially canceled by G()!
	// See page 16: http://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
    return (D * F * G) / (4 * max(dot(n, v), dot(n, l)));
}
// Calculates diffuse amount based on energy conservation
//
// diffuse - Diffuse amount
// specular - Specular color (including light color)
// metalness - surface metalness amount
//
// Metals should have an albedo of (0,0,0)...mostly
// See slide 65: http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
float3 DiffuseEnergyConserve(float3 diffuse, float3 specular, float metalness)
{
    return diffuse * ((1 - saturate(specular)) * (1 - metalness));
}
// Range-based attenuation function
float Attenuate(Light light, float3 worldPos)
{
    float dist = distance(light.Position, worldPos);

	// Ranged-based attenuation
    float att = saturate(1.0f - (dist * dist / (light.Range * light.Range)));

	// Soft falloff
    return att * att;
}
float ConeAttenuate(float3 toLight, Light light)
{
    // Calculate the spot falloff
    return pow(saturate(dot(-toLight, normalize(light.Direction))), light.SpotFalloff);
}


// === PHYSICALLY BASED LIGHTING LIGHTS ====================================
float3 DirectionalLight(Light light, float3 normal, float3 surfaceColor, float3 viewVector, float roughness, float3 specularColor, float metalness)
{
    // Calculate Light
    float3 toLight = normalize(-light.Direction);
    float diffuse = Lambert(normal, toLight);
    float3 specular = MicrofacetBRDF(normal, toLight, viewVector, roughness, specularColor);
    // Calculate diffuse with energy conservation, including diffuse for metals
    float3 balancedDiff = DiffuseEnergyConserve(diffuse, specular, metalness);
    // Combine the final diffuse and specular values for this light
    return (balancedDiff * surfaceColor + specular) * light.Intensity * light.Color;
}
float3 PointLight(float3 worldPosition, Light light, float3 normal, float3 surfaceColor, float3 viewVector, float roughness, float3 specularColor, float metalness)
{
    // Calculate Light
    float3 toLight = normalize(light.Position - worldPosition);
    float diffuse = Lambert(normal, toLight);
    float3 fresnelResult;
    float3 specular = MicrofacetBRDF(normal, toLight, viewVector, roughness, specularColor);
    // Calculate diffuse with energy conservation, including diffuse for metals
    float3 balancedDiff = DiffuseEnergyConserve(diffuse, specular, metalness);
    // Combine the final diffuse and specular values for this light
    return (balancedDiff * surfaceColor + specular) * light.Color * (light.Intensity * Attenuate(light, worldPosition));
}
float3 SpotLight(float3 worldPosition, Light light, float3 normal, float3 surfaceColor, float3 viewVector, float roughness, float3 specColor, float metalness)
{
    // Calculate Light
    float3 toLight = normalize(light.Position - worldPosition);
    float diffuse = Lambert(normal, toLight);
    float3 specular = MicrofacetBRDF(normal, toLight, viewVector, roughness, specColor);
    // Calculate diffuse with energy conservation, including diffuse for metals
    float3 balancedDiff = DiffuseEnergyConserve(diffuse, specular, metalness);
    // Combine the final diffuse and specular values for this light
    return (balancedDiff * surfaceColor + specular) * light.Color * (light.Intensity * Attenuate(light, worldPosition) * ConeAttenuate(toLight, light));
}


// assuming input values are not normalized
float4 totalLight(float3 normal, float3 worldPosition, float2 uv, float3 tangent)
{
    // Clean up un-normalized normals
    normal = normalize(normal);
    tangent = normalize(tangent);
    float3 viewVector = normalize(cameraPosition - worldPosition);
    
    // Alpha-Clipping
    //float alpha = hasOpacityMap ? OpacityMap.Sample(Sampler, uv).r : 1.0f;
    //alpha *= transparency; // Material's transparency value
    //clip(alpha - 0.1f);
    
    ///
    /// Texturing
    ///
    uv = uv * uvScale + uvOffset; // Scale and offset UVs
    float4 textureColor = AlbedoTexture.Sample(Sampler, uv);
    clip(textureColor.a * colorTint.a - 0.1); // Simple Alpha Culling
    float3 surfaceColor = pow(textureColor.rgb, 2.2f); // Surface color with gamma correction
    //surfaceColor = hasMask ? (surfaceColor * TextureMask.Sample(Sampler, uv).rgb) : surfaceColor; // Adjust surface color with mask
    
    //normal = hasNormalMap ? normalMapCalc(uv, normal, tangent) : normal;
    //float roughness = hasRoughMap ? RoughnessMap.Sample(Sampler, uv).r : 0.2f;
    //float metalness = hasMetalMap ? MetalnessMap.Sample(Sampler, uv).r : 0.0f;
    
    normal = NormalMapping(uv, normal, tangent);
    float roughness = RoughnessMap.Sample(Sampler, uv).r;
    float metalness = MetalMap.Sample(Sampler, uv).r;
    
    
    // Specular color determination -----------------
    // Assume albedo texture is actually holding specular color where metalness == 1
    // Note the use of lerp here - metal is generally 0 or 1, but might be in between
    // because of linear texture sampling, so we lerp the specular color to match
    float3 specularColor = lerp(F0_NON_METAL, surfaceColor.rgb, metalness);
    // Lighting
    //float shadowAmount = 1.0f;
    //if (hasShadowMap)
    //{
    //    // Perform the perspective divide (divide by W) ourselves
    //    // Convert the normalized device coordinates to UVs for sampling
    //    float2 shadowUV = shadowMapPos.xy / shadowMapPos.w * 0.5f + 0.5f;
    //    shadowUV.y = 1 - shadowUV.y; // Flip the Y
    //    // Grab the distances we need: light-to-pixel and closest-surface
    //    float distToLight = shadowMapPos.z / shadowMapPos.w;
    //    // Get a ratio of comparison results using SampleCmpLevelZero()
    //    shadowAmount = ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowUV, distToLight).r;
    //}
    
    // Light Calculations
    float3 totalLight = float3(0, 0, 0);
    for (int i = 0; i < lightCount; i++)
    {
        switch (lights[i].Type)
        {
            case LIGHT_TYPE_DIRECTIONAL:
                totalLight += DirectionalLight(lights[i], normal, surfaceColor, viewVector, roughness, specularColor, metalness);
                break;
            case LIGHT_TYPE_POINT:
                totalLight += PointLight(worldPosition, lights[i], normal, surfaceColor, viewVector, roughness, specularColor, metalness);
                break;
            case LIGHT_TYPE_SPOT:
                totalLight += SpotLight(worldPosition, lights[i], normal, surfaceColor, viewVector, roughness, specularColor, metalness);
                break;
        }
    }
    // Gamma Correct
    float3 finalColor = pow(totalLight, 1.0f / 2.2f);
    // EnvironmentMap Reflections
    //if (hasEnvironmentMap)
    //{
    //    float3 reflectionVector = reflect(-viewVector, normal); // Need camera to pixel vector, so negate
    //    float3 reflectionColor = EnvironmentMap.Sample(Sampler, reflectionVector).rgb;
	//    // Interpolate between the surface color and reflection color using a Fresnel term
    //    // Fresnel term - Schlick approx.
    //    // 
    //    // v - View vector
    //    // h - Half vector
    //    // f0 - Value when l = n
    //    //
    //    // F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5
    //     
    //    float3 h = normalize(viewVector + normalize(-light.Direction));
    //    finalColor = lerp(totalLight, reflectionColor, F_Schlick(viewVector, viewVector, F0_NON_METAL));
    //}
    return float4(finalColor * colorTint.rgb, 1.0f);
}

#endif