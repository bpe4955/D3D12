#include "Material.h"
#include "D3D12Helper.h"

using namespace DirectX;

Material::Material(
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState, 
	DirectX::XMFLOAT4 _colorTint,
	DirectX::XMFLOAT2 _uvOffset,
	DirectX::XMFLOAT2 _uvScale):
	pipelineState(_pipelineState),
	colorTint(_colorTint),
	uvOffset(_uvOffset),
	uvScale(_uvScale),
	materialTexturesFinalized(false),
	highestSRVSlot(-1)
{
	// Init remaining data
	finalGPUHandleForSRVs = {};
	ZeroMemory(textureSRVsBySlot, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * 128);
}

// Getters
Microsoft::WRL::ComPtr<ID3D12PipelineState> Material::GetPipelineState() { return pipelineState; } 
DirectX::XMFLOAT4 Material::GetColorTint() { return colorTint; } 
DirectX::XMFLOAT2 Material::GetUVOffset() { return uvOffset; }
DirectX::XMFLOAT2 Material::GetUVScale() { return uvScale; }
D3D12_GPU_DESCRIPTOR_HANDLE Material::GetFinalGPUHandleForTextures() { return finalGPUHandleForSRVs; }

// Setters
void Material::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState) { pipelineState = _pipelineState; }
void Material::SetColorTint(DirectX::XMFLOAT4 _colorTint) { colorTint = _colorTint; }
void Material::SetUVOffset(DirectX::XMFLOAT2 _uvOffset) { uvOffset = _uvOffset; }
void Material::AddUVOffset(DirectX::XMFLOAT2 _uvOffset) { uvOffset = XMFLOAT2(uvOffset.x + _uvOffset.x, uvOffset.y + _uvOffset.y); }
void Material::SetUVScale(DirectX::XMFLOAT2 _uvScale) { uvScale = _uvScale; }

// Functions

/// <summary>
/// Adds a texture (through its SRV descriptor) to the
/// material for the given slot (gpu register).  This method
/// does nothing if the slot is invalid or the material 
/// has already been finalized.
/// </summary>
/// <param name="srv">Handle to this texture's SRV</param>
/// <param name="slot">gpu register for this texture</param>
void Material::AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srv, int slot)
{
	if (materialTexturesFinalized || slot < 0 || slot >= 128) { return; }
	textureSRVsBySlot[slot] = srv;
	highestSRVSlot = max(highestSRVSlot, slot);
}

// --------------------------------------------------------
// Denotes that we're done adding textures to the material,
// meaning its safe to copy all of the texture SRVs from 
// their own descriptors to the final CBV/SRV descriptor
// heap so we can access them as a group while drawing.
// --------------------------------------------------------
void Material::FinalizeMaterial()
{
	if (materialTexturesFinalized) { return; }
	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();
	for (int i = 0; i <= highestSRVSlot; i++)
	{
		// Copy a single SRV at a time since they're all
		// currently in separate heaps!
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
			d3d12Helper.CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(textureSRVsBySlot[i], 1);

		// Save the first resulting handle
		if (i == 0) { finalGPUHandleForSRVs = gpuHandle; }
	}
	materialTexturesFinalized = true;
}
