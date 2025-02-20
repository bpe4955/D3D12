#include "Emitter.h"
#include "D3D12Helper.h"
#include "Assets.h"
// Helper macro for getting a float between min and max
#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)

//
// Construct
//

Emitter::Emitter(int _maxParticles, int _particlePerSecond,
	float _lifeTime,
	D3D12_CPU_DESCRIPTOR_HANDLE texture,
	bool _isAdditive,
	DirectX::XMFLOAT3 _emitterPosition,
	DirectX::XMFLOAT3 _positionRandomRange,
	DirectX::XMFLOAT3 _startVelocity,
	DirectX::XMFLOAT3 _velocityRandomRange,
	DirectX::XMFLOAT3 _emitterAcceleration,
	DirectX::XMFLOAT2 _rotationStartMinMax,
	DirectX::XMFLOAT2 _rotationEndMinMax,
	DirectX::XMFLOAT2 _sizeStartMinMax,
	DirectX::XMFLOAT2 _sizeEndMinMax,
	DirectX::XMFLOAT4 _startColor,
	DirectX::XMFLOAT4 _endColor) :
	maxParticles(_maxParticles),
	particles(nullptr),
	particlesPerSecond(_particlePerSecond),
	lifeTime(_lifeTime),
	isAdditive(_isAdditive),
	positionRandomRange(_positionRandomRange),
	startVelocity(_startVelocity),
	velocityRandomRange(_velocityRandomRange),
	acceleration(_emitterAcceleration),
	rotationStartMinMax(_rotationStartMinMax),
	rotationEndMinMax(_rotationEndMinMax),
	sizeStartMinMax(_sizeStartMinMax),
	sizeEndMinMax(_sizeEndMinMax),
	startColor(_startColor),
	endColor(_endColor),
	numLivingParticles(0),
	firstLivingIndex(0),
	firstDeadIndex(0),
	timeSinceLastEmit(0)
{
	// Variables
	secondsPerParticle = (float)1 / (float)_particlePerSecond;
	transform = std::make_shared<Transform>();
	transform->SetPosition(_emitterPosition);

	// Set up array and Resources
	textureGPUHandle =
		D3D12Helper::GetInstance().CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(texture, 1);
	CreateParticlesAndResources();
}

Emitter::~Emitter()
{
	delete[] particles;
}


//
// Getters
//
int Emitter::GetFirstLivingIndex() { return firstLivingIndex; }
int Emitter::GetFirstDeadIndex() { return firstDeadIndex; }
Particle* Emitter::GetParticles() { return particles; }
int Emitter::GetNumLivingParticles() { return numLivingParticles; }
std::shared_ptr<Transform> Emitter::GetTransform() { return transform; }
Microsoft::WRL::ComPtr<ID3D12PipelineState> Emitter::GetPipelineState() { return pipelineState; }
Microsoft::WRL::ComPtr<ID3D12RootSignature> Emitter::GetRootSignature() { return rootSig; }
Microsoft::WRL::ComPtr<ID3D12Resource> Emitter::GetBuffer() { return buffer; }
D3D12_INDEX_BUFFER_VIEW Emitter::GetIndexBufferView() { return ibView; }
D3D12_CPU_DESCRIPTOR_HANDLE Emitter::GetCPUHandle() { return structuredBuffCPUHandle; }
D3D12_GPU_DESCRIPTOR_HANDLE Emitter::GetGPUHandle() { return structuredBuffGPUHandle; }
D3D12_GPU_DESCRIPTOR_HANDLE Emitter::GetTextureGPUHandle() { return textureGPUHandle; }

//
// Setters
//
void Emitter::SetParticlesPerSecond(int _particlesPerSecond)
{
	particlesPerSecond = max(1,_particlesPerSecond);
	secondsPerParticle = 1.0f / (float)particlesPerSecond;
}

//void Emitter::SetMaxParticles(int _maxParticles)
//{
//	D3D12Helper::GetInstance().WaitForGPU();
//	maxParticles = max(1, _maxParticles);
//	CreateParticlesAndResources();
//
//	// Reset Emission Details
//	timeSinceLastEmit = 0.0f;
//	numLivingParticles = 0;
//	firstDeadIndex = 0;
//	firstLivingIndex = 0;
//}

// 
// Init
// 

void Emitter::CreateParticlesAndResources()
{
	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();

	// Delete and release existing resources
	if (particles) 
		delete[] particles;

	// Set up the particle array
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	// Create an index buffer for particle drawing
	// indices as if we had two triangles per particle
	int numIndices = maxParticles * 6;
	unsigned int* indices = new unsigned int[numIndices];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}
	indexBuffer = d3d12Helper.CreateStaticBuffer(sizeof(unsigned int), indexCount, indices);
	ibView = {};
	ibView.Format = DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = sizeof(unsigned int) * indexCount;
	ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	delete[] indices;

	// Create Buffer
	structuredBuffCPUHandle = d3d12Helper.CreateParticleBuffer(sizeof(Particle), maxParticles, buffer);
	structuredBuffGPUHandle = d3d12Helper.CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(structuredBuffCPUHandle, 1);

	// Create Pipeline Data
	Assets& assets = Assets::GetInstance();
	pipelineState = 
		isAdditive ? assets.GetPiplineState(L"PipelineStates/ParticleAdditive")
		: assets.GetPiplineState(L"PipelineStates/ParticleTransparent");
	rootSig = assets.GetRootSig(L"RootSigs/Particle");
}

//
// Draw
//

// https://alextardif.com/D3D11To12P2.html
// Might want to just use d3d12helper::FillNextConstantBufferAndGetGPUDescriptorHandle 
// if there are issues with buffer data changing between frames
void Emitter::CopyParticlesToGPU(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	Microsoft::WRL::ComPtr<ID3D12Device> device)
{
	// Get resource ready to copy
	// Transition the buffer to generic read
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = buffer.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &rb);
	
	// Now create an intermediate upload heap for copying data
	if (!uploadHeap)
	{
		D3D12_HEAP_PROPERTIES uploadProps = {};
		uploadProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadProps.CreationNodeMask = 1;
		uploadProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Can only ever be Generic_Read state
		uploadProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC desc = buffer->GetDesc();
		device->CreateCommittedResource(
			&uploadProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			0,
			IID_PPV_ARGS(uploadHeap.GetAddressOf()));
	}

	// Map resource
	void* mappedPtr = 0;
	uploadHeap->Map(0, NULL, &mappedPtr);

	// Copy Data
	if (firstLivingIndex < firstDeadIndex) // Contiguous particles
	{
		// Only copy from FirstAlive -> FirstDead
		memcpy(
			mappedPtr, // Destination = start of particle buffer
			particles + firstLivingIndex, // Source = particle array, offset to first living particle
			sizeof(Particle) * numLivingParticles); // Amount = number of particles (measured in BYTES!)
	}
	else
	{
		// Copy from 0 -> FirstDead
		memcpy(
			mappedPtr, // Destination = start of particle buffer
			particles, // Source = start of particle array
			sizeof(Particle) * firstDeadIndex); // Amount = particles up to first dead (measured in BYTES!)
		// ALSO copy from FirstAlive -> End
		memcpy(
			(void*)((Particle*)mappedPtr + firstDeadIndex), // Destination = particle buffer, AFTER the data we copied in previous memcpy()
			particles + firstLivingIndex, // Source = particle array, offset to first living particle
			sizeof(Particle) * (maxParticles - firstLivingIndex)); // Amount = number of living particles at end of array (measured in BYTES!)
	}

	// Unmap resource
	uploadHeap->Unmap(0, NULL);

	// Copy the whole buffer from uploadheap to vert buffer
	commandList->CopyResource(buffer.Get(), uploadHeap.Get());
	
	// Transition the buffer to shader resource
	rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = buffer.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &rb);
}

// 
// Update
// 

/// <summary>
/// Check for dead particles and emit new ones
/// </summary>
/// <param name="delta">Time since last update</param>
/// <param name="currentTime"></param>
void Emitter::Update(float dt, float currentTime)
{
	// Update Living Particles
	if (numLivingParticles > 0)
	{
		// Contiguous living particles
		if (firstLivingIndex < firstDeadIndex)
		{
			for (int i = firstLivingIndex; i < firstDeadIndex; i++)
				if (UpdateCurrentParticle(currentTime, i)) // Early break once particles are alive
					break;
		}
		// Non-contiguous living particles
		else /* if (firstLivingIndex < firstDeadIndex) */
		{
			// Oldest particles first
			for (int i = firstLivingIndex; i < maxParticles; i++)
				if (UpdateCurrentParticle(currentTime, i)) // Early break once particles are alive
					break;
			// Newer half
			for(int i=0; i<firstDeadIndex; i++)
				if (UpdateCurrentParticle(currentTime, i)) // Early break once particles are alive
					break;
		}
	}

	// Add to the time
	timeSinceLastEmit += dt;

	// Emit particles
	while (timeSinceLastEmit > secondsPerParticle)
	{
		EmitParticle(currentTime);
		timeSinceLastEmit -= secondsPerParticle;
	}
}
/// <summary>
/// Mark particles as dead if they have existed for too long
/// </summary>
/// <param name="currentTime"></param>
/// <param name="index">Index of particle in particles[]</param>
/// <returns>False if particle is dead, true if alive</returns>
bool Emitter::UpdateCurrentParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;
	if (age >= lifeTime)
	{
		firstLivingIndex++;
		firstLivingIndex %= maxParticles;
		numLivingParticles--;
		return false;
	}

	return true;
}
/// <summary>
/// Create a new particle
/// </summary>
void Emitter::EmitParticle(float currentTime)
{
	// Check if new particle should be emitted
	if (numLivingParticles == maxParticles)
		return;

	// Get spawn location
	int spawnIndex = firstDeadIndex;

	// Give Particle Data
	particles[spawnIndex].EmitTime = currentTime;
	particles[spawnIndex].StartPos = transform->GetPosition();
	particles[spawnIndex].StartPos.x += positionRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnIndex].StartPos.y += positionRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnIndex].StartPos.z += positionRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust particle start velocity based on random range
	particles[spawnIndex].StartVel = startVelocity;
	particles[spawnIndex].StartVel.x += velocityRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnIndex].StartVel.y += velocityRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnIndex].StartVel.z += velocityRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust start and end rotation values based on range
	particles[spawnIndex].StartRot = RandomRange(rotationStartMinMax.x, rotationStartMinMax.y);
	particles[spawnIndex].EndRot = RandomRange(rotationEndMinMax.x, rotationEndMinMax.y);

	// Adjust start and end size values based on range
	particles[spawnIndex].StartSize = RandomRange(sizeStartMinMax.x, sizeStartMinMax.y);
	particles[spawnIndex].EndSize = RandomRange(sizeEndMinMax.x, sizeEndMinMax.y);

	// Increment living count
	numLivingParticles++;

	// Increment Dead Index
	firstDeadIndex++;
	firstDeadIndex %= maxParticles;
}

