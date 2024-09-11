#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
class Material
{
public:
	Material(
		Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState,
		DirectX::XMFLOAT4 _colorTint = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT2 _uvOffset = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 _uvScale = DirectX::XMFLOAT2(1, 1));

	// Getters
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();
	DirectX::XMFLOAT4 GetColorTint();
	DirectX::XMFLOAT2 GetUVOffset();
	DirectX::XMFLOAT2 GetUVScale();
	D3D12_GPU_DESCRIPTOR_HANDLE GetFinalGPUHandleForTextures();

	// Setters
	void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState);
	void SetColorTint(DirectX::XMFLOAT4 _colorTint);
	void SetUVOffset(DirectX::XMFLOAT2 _uvOffset);
	void AddUVOffset(DirectX::XMFLOAT2 _uvOffset);
	void SetUVScale(DirectX::XMFLOAT2  _uvScale);

	// Functions
	void AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srv, int slot);
	void FinalizeMaterial();

private:
	// Pipeline state, which can be shared among materials
	// This also includes shaders
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	bool materialTexturesFinalized;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSRVsBySlot[128];
	int highestSRVSlot;
	D3D12_GPU_DESCRIPTOR_HANDLE finalGPUHandleForSRVs;

	// Material Properties
	DirectX::XMFLOAT4 colorTint;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;
};

