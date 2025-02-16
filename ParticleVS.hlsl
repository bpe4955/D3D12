// Define a single Particle
// Make sure it's padded accordingly
struct Particle
{
    float EmitTime;
    float3 StartPosition;
    
    float3 StartVelocity;
    float padding;
};

cbuffer PerFrame : register(b0)
{
    matrix view;
    matrix projection;
}

cbuffer ExternalData : register(b1)
{
    float currentTime;
    float3 acceleration;
    
    float lifeTime;
}

StructuredBuffer<Particle> ParticleData : register(t0); // Maybe u0 instead

// Defines the output data of our vertex shader
struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
    VertexToPixel output;

	// Get id info
    uint particleID = id / 4; // Every group of 4 verts are ONE particle!
    uint cornerID = id % 4; // 0,1,2,3 = the corner of the particle "quad"

	// Grab one particle
    Particle p = ParticleData.Load(particleID);
    
    // Calculate the age and age "percentage" (0 to 1)
    float age = currentTime - p.EmitTime;
    float agePercent = age / lifeTime;
    
    // Constant accleration function to determine the particle's
	// current location based on age, start velocity and accel
    float3 pos = acceleration * (age * age / 2.0f) + p.StartVelocity * age + p.StartPosition;
    
    // Offsets for the 4 corners of a quad - we'll only
	// use one for each vertex, but which one depends
	// on the cornerID above.
    float2 offsets[4];
    offsets[0] = float2(-1.0f, +1.0f); // TL
    offsets[1] = float2(+1.0f, +1.0f); // TR
    offsets[2] = float2(+1.0f, -1.0f); // BR
    offsets[3] = float2(-1.0f, -1.0f); // BL
    
    // Billboarding!
	// Offset the position based on the camera's right and up vectors
    pos += float3(view._11, view._12, view._13) * offsets[cornerID].x; // RIGHT
    pos += float3(view._21, view._22, view._23) * offsets[cornerID].y; // UP
    
    // Calculate output position
    matrix viewProj = mul(projection, view);
    output.position = mul(viewProj, float4(pos, 1.0f));
    
    // Set UVs
    float2 uvs[4];
    uvs[0] = float2(0, 0); // TL
    uvs[1] = float2(1, 0); // TR
    uvs[2] = float2(1, 1); // BR
    uvs[3] = float2(0, 1); // BL
    output.uv = uvs[cornerID];

    
    return output;
}