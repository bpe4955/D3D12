#include "Sky.h"
#include <WICTextureLoader.h>
#include "Assets.h"
#include "D3D12Helper.h"

Sky::Sky(std::wstring filePath,
	DirectX::XMFLOAT4 _colorTint) :
	colorTint(_colorTint)
{
	Assets& assets = Assets::GetInstance();

	pipelineState = assets.GetPiplineState(L"PipelineStates/Sky");
	mesh = assets.GetMesh(L"Basic Meshes/cube");

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = assets.GetTexture(filePath, false, true);
	textureGPUHandle =
		D3D12Helper::GetInstance().CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(cpuHandle, 1);
}
Sky::~Sky() {}


// Getters + Setters
DirectX::XMFLOAT4 Sky::GetColorTint() { return colorTint; }
void Sky::SetColorTint(DirectX::XMFLOAT4 _colorTint) { colorTint = _colorTint; }
D3D12_GPU_DESCRIPTOR_HANDLE Sky::GetTextureGPUHandle() { return textureGPUHandle; }
std::shared_ptr<Mesh> Sky::GetMesh() { return mesh; }
Microsoft::WRL::ComPtr<ID3D12PipelineState> Sky::GetPipelineState() { return pipelineState; }


