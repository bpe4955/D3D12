#include "Graphics.h"
#include "D3D12Helper.h"

// Tell the drivers to use high-performance GPU in multi-GPU systems (like laptops)
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; // NVIDIA
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

namespace Graphics
{

	// Annonymous namespace to hold variables
	// only accessible in this file
	namespace
	{
		bool apiInitialized = false;
		bool supportsTearing = false;
		bool vsyncDesired = false;
		BOOL isFullscreen = false;
		D3D_FEATURE_LEVEL featureLevel;
		Microsoft::WRL::ComPtr<ID3D12InfoQueue> InfoQueue;
	}
}

// Getters
bool Graphics::VsyncState() { return vsyncDesired || !supportsTearing || isFullscreen; }
std::wstring Graphics::APIName()
{
	switch (featureLevel)
	{
	case D3D_FEATURE_LEVEL_10_0: return L"D3D10";
	case D3D_FEATURE_LEVEL_10_1: return L"D3D10.1";
	case D3D_FEATURE_LEVEL_11_0: return L"D3D11";
	case D3D_FEATURE_LEVEL_11_1: return L"D3D11.1";
	case D3D_FEATURE_LEVEL_12_0: return L"D3D12";
	case D3D_FEATURE_LEVEL_12_1: return L"D3D12.1";
	default: return L"Unknown";
	}
}

// --------------------------------------------------------
// Initializes the Graphics API, which requires window details.
// 
// windowWidth     - Width of the window (and our viewport)
// windowHeight    - Height of the window (and our viewport)
// windowHandle    - OS-level handle of the window
// vsyncIfPossible - Sync to the monitor's refresh rate if available?
// --------------------------------------------------------
HRESULT Graphics::Initialize(unsigned int windowWidth, unsigned int windowHeight, HWND windowHandle, bool vsyncIfPossible)
{
	// Only initialize once
	if (apiInitialized)
		return E_FAIL;

	apiInitialized = false;
	supportsTearing = false;
	vsyncDesired = false;
	isFullscreen = false;

	// Save desired vsync state, though it may be stuck "on" if
	// the device doesn't support screen tearing
	vsyncDesired = vsyncIfPossible;

	// Determine if screen tearing ("vsync off") is available
	// - This is necessary due to variable refresh rate displays
	Microsoft::WRL::ComPtr<IDXGIFactory5> factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
	{
		// Check for this specific feature (must use BOOL typedef here!)
		BOOL tearingSupported = false;
		HRESULT featureCheck = factory->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&tearingSupported,
			sizeof(tearingSupported));

		// Final determination of support
		supportsTearing = SUCCEEDED(featureCheck) && tearingSupported;
	}

	// D3D12 variables
	featureLevel = D3D_FEATURE_LEVEL_12_0;
	currentSwapBuffer = 0;
	rtvDescriptorSize = 0;
	dsvHandle = {};
	for (int i = 0; i < numBackBuffers; i++) { rtvHandles[i] = {}; }
	scissorRect = {};


#if defined(DEBUG) || defined(_DEBUG)
	// If we're in debug mode in visual studio, we also
	// want to make a "Debug DirectX Device" to see some
	// errors and warnings in Visual Studio's output window
	// when things go wrong!
	ID3D12Debug* debugController;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
	debugController->EnableDebugLayer();
#endif

	// Result variable for below function calls
	HRESULT hr = S_OK;

	// Create the DX 12 device and check the feature level
	{
		hr = D3D12CreateDevice(
			0, // Not explicitly specifying which adapter (GPU)
			D3D_FEATURE_LEVEL_11_0, // MINIMUM feature level - NOT the level we'll turn on
			IID_PPV_ARGS(Device.GetAddressOf())); // Macro to grab necessary IDs of device
		if (FAILED(hr)) return hr;
		// Now that we have a device, determine the maximum feature level supported by the device
		D3D_FEATURE_LEVEL levelsToCheck[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1
		};
		D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
		levels.pFeatureLevelsRequested = levelsToCheck;
		levels.NumFeatureLevels = ARRAYSIZE(levelsToCheck);
		Device->CheckFeatureSupport(
			D3D12_FEATURE_FEATURE_LEVELS,
			&levels,
			sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS));
		featureLevel = levels.MaxSupportedFeatureLevel;
	}

	// Set up DX12 command allocator / queue / list,
	// which are necessary pieces for issuing standard API calls
	{
		// Set up allocator
		for (unsigned int i = 0; i < numBackBuffers; i++)
		{
			// Set up allocators
			Device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(commandAllocators[i].GetAddressOf()));
		}
		// Command queue
		D3D12_COMMAND_QUEUE_DESC qDesc = {};
		qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		Device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));
		// Command list
		Device->CreateCommandList(
			0, // Which physical GPU will handle these tasks? 0 for single GPU setup
			D3D12_COMMAND_LIST_TYPE_DIRECT, // Type of list - direct is for standard API calls
			commandAllocators[0].Get(), // The allocator for this list
			0, // Initial pipeline state - none for now
			IID_PPV_ARGS(commandList.GetAddressOf()));
	}

	// Now that we have a device and a command list,
	// we can initialize the DX12 helper singleton, which will
	// also create a fence for synchronization
	{
		D3D12Helper::GetInstance().Initialize(
			Device,
			commandList,
			commandQueue,
			commandAllocators,
			numBackBuffers);
	}

	// Swap chain creation
	{
		// Create a description of how our swap chain should work
		DXGI_SWAP_CHAIN_DESC swapDesc = {};
		swapDesc.BufferCount = numBackBuffers;
		swapDesc.BufferDesc.Width = windowWidth;
		swapDesc.BufferDesc.Height = windowHeight;
		swapDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.Flags = supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		swapDesc.OutputWindow = windowHandle;
		swapDesc.SampleDesc.Count = 1;
		swapDesc.SampleDesc.Quality = 0;
		swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapDesc.Windowed = true;
		// Create a DXGI factory, which is what we use to create a swap chain
		Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory;
		CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
		hr = dxgiFactory->CreateSwapChain(commandQueue.Get(), &swapDesc, swapChain.GetAddressOf());
	}

	// Create back buffers
	{
		// What is the increment size between RTV descriptors in a
		// descriptor heap? This differs per GPU so we need to
		// get it at applications start up
		rtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		// First create a descriptor heap for RTVs
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = numBackBuffers;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf()));
		// Now create the RTV handles for each buffer (buffers were created by the swap chain)
		for (unsigned int i = 0; i < numBackBuffers; i++)
		{
			// Grab this buffer from the swap chain
			swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffers[i].GetAddressOf()));
			// Make a handle for it
			rtvHandles[i] = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandles[i].ptr += rtvDescriptorSize * i;
			// Create the render target view
			Device->CreateRenderTargetView(backBuffers[i].Get(), 0, rtvHandles[i]);
		}
	}

	// Create depth/stencil buffer
	{
		// Create a descriptor heap for DSV
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf()));
		// Describe the depth stencil buffer resource
		D3D12_RESOURCE_DESC depthBufferDesc = {};
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.Height = windowHeight;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Width = windowWidth;
		// Describe the clear value that will most often be used
		// for this buffer (which optimizes the clearing of the buffer)
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clear.DepthStencil.Depth = 1.0f;
		clear.DepthStencil.Stencil = 0;
		// Describe the memory heap that will house this resource
		D3D12_HEAP_PROPERTIES props = {};
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.CreationNodeMask = 1;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.VisibleNodeMask = 1;
		// Actually create the resource, and the heap in which it
		// will reside, and map the resource to that heap
		Device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clear,
			IID_PPV_ARGS(depthStencilBuffer.GetAddressOf()));
		// Get the handle to the Depth Stencil View that we'll
		// be using for the depth buffer. The DSV is stored in
		// our DSV-specific descriptor Heap.
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		// Actually make the DSV
		Device->CreateDepthStencilView(
			depthStencilBuffer.Get(),
			0, // Default view (first mip)
			dsvHandle);
	}

	// Set up the viewport so we render into the correct
	// portion of the render target
	viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)windowWidth;
	viewport.Height = (float)windowHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	// Define a scissor rectangle that defines a portion of
	// the render target for clipping. This is different from
	// a viewport in that it is applied after the pixel shader.
	// We need at least one of these, but we're rendering to
	// the entire window, so it'll be the same size.
	scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = windowWidth;
	scissorRect.bottom = windowHeight;

	// We're set up
	apiInitialized = true;

	// Call ResizeBuffers(), which will also set up the 
	// render target view and depth stencil view for the
	// various buffers we need for rendering. This call 
	// will also set the appropriate viewport.
	//ResizeBuffers(windowWidth, windowHeight);

	// Wait for the GPU to catch up
	D3D12Helper::GetInstance().WaitForGPU();

	return S_OK;
}

// --------------------------------------------------------
// Called at the end of the program to clean up any
// graphics API specific memory. 
// 
// This exists for completeness since D3D objects generally
// use ComPtrs, which get cleaned up automatically.  Other
// APIs might need more explicit clean up.
// --------------------------------------------------------
void Graphics::ShutDown()
{
	delete& D3D12Helper::GetInstance();
}


// --------------------------------------------------------
// When the window is resized, the underlying 
// buffers (textures) must also be resized to match.
//
// If we don't do this, the window size and our rendering
// resolution won't match up.  This can result in odd
// stretching/skewing.
// 
// width  - New width of the window (and our viewport)
// height - New height of the window (and our viewport)
// --------------------------------------------------------
void Graphics::ResizeBuffers(unsigned int windowWidth, unsigned int windowHeight)
{
	// Ensure graphics API is initialized
	if (!apiInitialized)
		return;

	// Wait for the GPU to finish all work, since we'll
	// be destroying and recreating resources
	D3D12Helper::GetInstance().WaitForGPU();

	// Release the back buffers using ComPtr's Reset()
	for (unsigned int i = 0; i < numBackBuffers; i++)
		backBuffers[i].Reset();

	// Resize the swap chain (assuming a basic color format here)
	swapChain->ResizeBuffers(
		numBackBuffers,
		windowWidth,
		windowHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	// Go through the steps to setup the back buffers again
	// Note: This assumes the descriptor heap already exists
	// and that the rtvDescriptorSize was previously set
	for (unsigned int i = 0; i < numBackBuffers; i++)
	{
		// Grab this buffer from the swap chain
		swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffers[i].GetAddressOf()));
		// Make a handle for it
		rtvHandles[i] = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandles[i].ptr += rtvDescriptorSize * i;
		// Create the render target view
		Device->CreateRenderTargetView(backBuffers[i].Get(), 0, rtvHandles[i]);
	}
	// Reset back to the first back buffer
	currentSwapBuffer = 0;
	D3D12Helper::GetInstance().ResetFrameSyncCounters();
	// Reset the depth buffer and create it again
	{
		depthStencilBuffer.Reset();
		// Describe the depth stencil buffer resource
		D3D12_RESOURCE_DESC depthBufferDesc = {};
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.Height = windowHeight;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Width = windowWidth;
		// Describe the clear value that will most often be used
		// for this buffer (which optimizes the clearing of the buffer)
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clear.DepthStencil.Depth = 1.0f;
		clear.DepthStencil.Stencil = 0;
		// Describe the memory heap that will house this resource
		D3D12_HEAP_PROPERTIES props = {};
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.CreationNodeMask = 1;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.VisibleNodeMask = 1;
		// Actually create the resource, and the heap in which it
		// will reside, and map the resource to that heap
		Device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clear,
			IID_PPV_ARGS(depthStencilBuffer.GetAddressOf()));
		// Now recreate the depth stencil view
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		Device->CreateDepthStencilView(
			depthStencilBuffer.Get(),
			0, // Default view (first mip)
			dsvHandle);
	}

	// Recreate the viewport and scissor rects, too,
	// since the window size has changed
	{
		// Update the viewport and scissor rect so we render into the correct
		// portion of the render target
		viewport.Width = (float)windowWidth;
		viewport.Height = (float)windowHeight;
		scissorRect.right = windowWidth;
		scissorRect.bottom = windowHeight;
	}
	// Are we in a fullscreen state?
	swapChain->GetFullscreenState(&isFullscreen, 0);
	// Wait for the GPU before we proceed
	D3D12Helper::GetInstance().WaitForGPU();
}


// --------------------------------------------------------
// Prints graphics debug messages waiting in the queue
// --------------------------------------------------------
void Graphics::PrintDebugMessages()
{
	// Do we actually have an info queue (usually in debug mode)
	if (!InfoQueue)
		return;

	// Any messages?
	UINT64 messageCount = InfoQueue->GetNumStoredMessages();
	if (messageCount == 0)
		return;

	// Loop and print messages
	for (UINT64 i = 0; i < messageCount; i++)
	{
		// Get the size so we can reserve space
		size_t messageSize = 0;
		InfoQueue->GetMessage(i, 0, &messageSize);

		// Reserve space for this message
		D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageSize);
		InfoQueue->GetMessage(i, message, &messageSize);

		// Print and clean up memory
		if (message)
		{
			// Color code based on severity
			switch (message->Severity)
			{
			case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			case D3D12_MESSAGE_SEVERITY_ERROR:
				printf("\x1B[91m"); break; // RED

			case D3D12_MESSAGE_SEVERITY_WARNING:
				printf("\x1B[93m"); break; // YELLOW

			case D3D12_MESSAGE_SEVERITY_INFO:
			case D3D12_MESSAGE_SEVERITY_MESSAGE:
				printf("\x1B[96m"); break; // CYAN
			}

			printf("%s\n\n", message->pDescription);
			free(message);

			// Reset color
			printf("\x1B[0m");
		}
	}

	// Clear any messages we've printed
	InfoQueue->ClearStoredMessages();
}
