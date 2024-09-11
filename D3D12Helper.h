#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

class D3D12Helper
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static D3D12Helper& GetInstance()
	{
		if (!instance)
		{
			instance = new D3D12Helper();
		}

		return *instance;
	}

	// Remove these functions (C++ 11 version)
	D3D12Helper(D3D12Helper const&) = delete;
	void operator=(D3D12Helper const&) = delete;

private:
	static D3D12Helper* instance;
	D3D12Helper() {};
#pragma endregion
public:
	~D3D12Helper();
	// Initialization for singleton
	void Initialize(
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>* commandAllocators,
		unsigned int numBackBuffers);
	// Resource creation
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateStaticBuffer(
		unsigned int dataStride,
		unsigned int dataCount,
		void* data);
	// Constant Buffer heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetCBVSRVDescriptorHeap();
	D3D12_GPU_DESCRIPTOR_HANDLE FillNextConstantBufferAndGetGPUDescriptorHandle(
		void* data,
		unsigned int dataSizeInBytes);
	// Command list & synchronization
	void ExecuteCommandList();
	void WaitForGPU();
	void ResetFrameSyncCounters();
	unsigned int SyncSwapChain(unsigned int currentSwapBufferIndex);
	// Texture Loading
	D3D12_CPU_DESCRIPTOR_HANDLE LoadTexture(const wchar_t* file, bool generateMips = true);
	D3D12_CPU_DESCRIPTOR_HANDLE LoadTextureDDS(const wchar_t* file, bool generateMips = true, bool isCubeMap = false);
	D3D12_GPU_DESCRIPTOR_HANDLE CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(
		D3D12_CPU_DESCRIPTOR_HANDLE firstDescriptorToCopy,
		unsigned int numDescriptorsToCopy);
private:
	// Overall device
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	// Command list related
	// Note: We're assuming a single command list for the entire
	// engine at this point. That's not always true for more
	// complex engines but should be fine for now
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>* commandAllocators = {};
	// Basic CPU/GPU synchronization
	unsigned int numBackBuffers = 3;
	Microsoft::WRL::ComPtr<ID3D12Fence> waitFence = {};
	HANDLE waitFenceEvent = {};
	unsigned long waitFenceCounter = 0;

	// Maximum number of constant buffers, assuming each buffer
	// is 256 bytes or less. Larger buffers are fine, but will
	// result in fewer buffers in use at any time
	const unsigned int maxConstantBuffers = 1000;
	// GPU-side constant buffer upload heap
	Microsoft::WRL::ComPtr<ID3D12Resource> cbUploadHeap;
	UINT64 cbUploadHeapSizeInBytes = 0;
	UINT64 cbUploadHeapOffsetInBytes = 0;
	void* cbUploadHeapStartAddress = 0;
	// GPU-side CBV/SRV descriptor heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvDescriptorHeap;
	SIZE_T cbvSrvDescriptorHeapIncrementSize = 0;
	unsigned int cbvDescriptorOffset = 0;
	void CreateConstantBufferUploadHeap();
	void CreateCBVSRVDescriptorHeap();
	// Frame sync'ing
	Microsoft::WRL::ComPtr<ID3D12Fence> frameSyncFence = {};
	HANDLE								frameSyncFenceEvent = {};
	UINT64*								frameSyncFenceCounters = 0;

	// Maximum number of texture descriptors (SRVs) we can have.
	// Each material will have a chunk of this,
	// Note: If we delayed the creation of this heap until
	// after all textures and materials were created,
	// we could come up with an exact amount. The following
	// constant ensures we (hopefully) never run out of room.
	const unsigned int maxTextureDescriptors = 1000;
	unsigned int srvDescriptorOffset = 0;
	// Texture resources we need to keep alive
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> cpuSideTextureDescriptorHeaps;
};

