#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <functional>

enum class Visibility {
	Invisible = 0,
	Opaque = 1,
	Transparent = 2
};

class Material
{
public:
	Material(
		Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootsignature,
		Visibility _visibility = Visibility::Opaque,
		DirectX::XMFLOAT4 _colorTint = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT2 _uvOffset = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 _uvScale = DirectX::XMFLOAT2(1, 1),
		D3D_PRIMITIVE_TOPOLOGY _topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	// Getters
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature();
	D3D_PRIMITIVE_TOPOLOGY GetTopology();
	DirectX::XMFLOAT4 GetColorTint();
	DirectX::XMFLOAT2 GetUVOffset();
	DirectX::XMFLOAT2 GetUVScale();
	D3D12_GPU_DESCRIPTOR_HANDLE GetFinalGPUHandleForTextures();
	bool HasTexture(int slot);
	float& GetRoughness();
	Visibility GetVisibility();

	// Setters
	void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState);
	void SetRootSig(Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSig);
	void SetTopology(D3D_PRIMITIVE_TOPOLOGY _topology);
	void SetColorTint(DirectX::XMFLOAT4 _colorTint);
	void SetUVOffset(DirectX::XMFLOAT2 _uvOffset);
	void AddUVOffset(DirectX::XMFLOAT2 _uvOffset);
	void SetUVScale(DirectX::XMFLOAT2  _uvScale);
	void SetRoughness(float _roughness);
	void SetVisibility(Visibility _visibility);
	void SetDirtyFunction(std::function<void()> funcPtr);

	// Functions
	void AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srv, int slot);
	void FinalizeMaterial();

private:
	// Pipeline state, which can be shared among materials
	// This also includes shaders
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	D3D_PRIMITIVE_TOPOLOGY topology;

	bool materialTexturesFinalized;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSRVsBySlot[128];
	int highestSRVSlot;
	D3D12_GPU_DESCRIPTOR_HANDLE finalGPUHandleForSRVs;

	// Material Properties
	DirectX::XMFLOAT4 colorTint;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;
	float roughness;
	Visibility visibility;

	// Delegates and Callbacks
	std::function<void()> dirtyCallBack;
};

