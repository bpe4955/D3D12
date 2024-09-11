#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include <memory>
class Sky
{
public:
	Sky(std::wstring filePath,
		DirectX::XMFLOAT4 _colorTint = DirectX::XMFLOAT4(1.f,1.f,1.f,1.f));
	~Sky();

	// Getters + Setters
	DirectX::XMFLOAT4 GetColorTint();
	void SetColorTint(DirectX::XMFLOAT4 _colorTint);
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();
	std::shared_ptr<Mesh> GetMesh();
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGPUHandle();

private:
	// Pipeline state, which can be shared among Skys
	// This also includes shaders
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	std::shared_ptr<Mesh> mesh;
	//D3D12_CPU_DESCRIPTOR_HANDLE textureCPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE textureGPUHandle;

	DirectX::XMFLOAT4 colorTint;

};

