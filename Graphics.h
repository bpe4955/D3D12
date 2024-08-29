#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace Graphics
{
	// --- GLOBAL VARS ---

	inline static const unsigned int numBackBuffers = 2;
	inline unsigned int currentSwapBuffer;
	inline Microsoft::WRL::ComPtr<ID3D12Device> Device;
	inline Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	inline Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	inline Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	inline Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	inline size_t rtvDescriptorSize;
	inline Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	inline Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;
	inline D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[numBackBuffers];
	inline D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	inline Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers[numBackBuffers];
	inline Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;
	inline D3D12_VIEWPORT viewport;
	inline D3D12_RECT scissorRect;

	// --- FUNCTIONS ---

	// Getters
	bool VsyncState();
	std::wstring APIName();

	// General functions
	HRESULT Initialize(unsigned int windowWidth, unsigned int windowHeight, HWND windowHandle, bool vsyncIfPossible);
	void ShutDown();
	void ResizeBuffers(unsigned int width, unsigned int height);

	// Debug Layer
	void PrintDebugMessages();
}