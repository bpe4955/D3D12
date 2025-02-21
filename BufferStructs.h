#pragma once
#include <DirectXMath.h>

// Must match the MAX_LIGHTS definition in your shaders
#define MAX_LIGHTS 128

// These should also match lights in shaders
#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

// Defines a single light that can be sent to the GPU
// Note: This must match light struct in shaders
//       and must also be a multiple of 16 bytes!
struct Light
{
	int					Type;
	DirectX::XMFLOAT3	Direction;	// 16 bytes

	float				Range;
	DirectX::XMFLOAT3	Position;	// 32 bytes

	float				Intensity;
	DirectX::XMFLOAT3	Color;		// 48 bytes

	float				SpotFalloff; // Bigger Value = Smaller circle
	DirectX::XMFLOAT3	Padding;	// 64 bytes
};


// This needs to match the expected per-frame vertex shader data
struct VSPerFrameData
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

struct VSPerObjectData
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldInvTranspose;
};

struct VSEmitterPerFrameData
{
	float currentTime;
	DirectX::XMFLOAT3 acceleration;

	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;

	float lifeTime;
	bool constrainYAxis;
	DirectX::XMFLOAT2 padding;
};

struct PSPerFrameData
{
	DirectX::XMFLOAT3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
	DirectX::XMFLOAT4 ambient;
};

struct PSPerMaterialData
{
	DirectX::XMFLOAT4 colorTint;
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
};

struct SkyPSData
{
	DirectX::XMFLOAT4 colorTint;
};

