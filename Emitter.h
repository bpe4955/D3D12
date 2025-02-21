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
	float StartRot;

	float EndRot;
	float StartSize;
	float EndSize;
	float padding;
};

class Emitter
{
public:
	Emitter(int _maxParticles, int particlePerSecond,
		float _lifeTime,
		D3D12_CPU_DESCRIPTOR_HANDLE texture,
		bool _isAdditive = true,
		bool _constrainYAxis = false,
		DirectX::XMFLOAT3 _emitterPosition = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _positionRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _startVelocity = DirectX::XMFLOAT3(0, 1.0f, 0),  // left/right, down/up, front/back
		DirectX::XMFLOAT3 _velocityRandomRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 _emitterAcceleration = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT2 _rotationStartMinMax = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 _rotationEndMinMax = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 _sizeStartMinMax = DirectX::XMFLOAT2(1.0f, 1.0f),
		DirectX::XMFLOAT2 _sizeEndMinMax = DirectX::XMFLOAT2(1.0f, 1.0f),
		DirectX::XMFLOAT4 _startColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT4 _endColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
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
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGPUHandle();

	// Setters
	void SetParticlesPerSecond(int _particlesPerSecond);
	//void SetMaxParticles(int _maxParticles);

	// Particle base values
	float lifeTime;
	DirectX::XMFLOAT3 acceleration;
	DirectX::XMFLOAT3 startVelocity;
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	bool constrainYAxis;

	// Particle randomization ranges
	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT2 rotationStartMinMax;
	DirectX::XMFLOAT2 rotationEndMinMax;
	DirectX::XMFLOAT2 sizeStartMinMax;
	DirectX::XMFLOAT2 sizeEndMinMax;

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
	bool isAdditive;

	// Particle Creation
	int particlesPerSecond;
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
	D3D12_GPU_DESCRIPTOR_HANDLE textureGPUHandle;

	// Init
	void CreateParticlesAndResources();
	
	// Update
	bool UpdateCurrentParticle(float currentTime, int index);
	void EmitParticle(float currentTime);

};

