#pragma once

#include <d3d12.h>
#include <memory>

#include "Transform.h"
#include "Material.h"

struct Particle
{
	float EmitTime;
	DirectX::XMFLOAT3 StartPos;

	DirectX::XMFLOAT3 StartVel;
	float padding;
};

class Emitter
{
public:
	Emitter(int _maxParticles, int particlePerSecond, 
		float _lifeTime,
		DirectX::XMFLOAT3 _emitterPosition = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _positionRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _startVelocity = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 _velocityRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _emitterAcceleration = DirectX::XMFLOAT3(0, 0, 0));
	~Emitter();

	// Getters
	int GetFirstLivingIndex();
	int GetFirstDeadIndex();
	Particle* GetParticles();
	int GetNumLivingParticles();
	std::shared_ptr<Transform> GetTransform();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature();
	Microsoft::WRL::ComPtr<ID3D12Resource> GetBuffer();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle();

	// Particle base values
	float lifeTime;
	DirectX::XMFLOAT3 acceleration;
	DirectX::XMFLOAT3 startVelocity;

	// Particle randomization ranges
	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT2 rotationStartMinMax;
	DirectX::XMFLOAT2 rotationEndMinMax;

	// Update
	void Update(float delta, float currentTime);

	// Draw
	void CopyParticlesToGPU(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		Microsoft::WRL::ComPtr<ID3D12Device> device);

private:
	// Particle Array Data
	int maxParticles;
	Particle* particles;
	int numLivingParticles;
	int firstLivingIndex;
	int firstDeadIndex;

	// Particle Creation
	int particlePerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;

	// General
	std::shared_ptr<Transform> transform;

	// Buffer Data
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
	D3D12_CPU_DESCRIPTOR_HANDLE structuredBuffCPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE structuredBuffGPUHandle;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
	D3D12_INDEX_BUFFER_VIEW ibView;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap;

	// Init
	void CreateParticlesAndResources();
	
	// Update
	bool UpdateCurrentParticle(float currentTime, int index);
	void EmitParticle(float currentTime);

};

