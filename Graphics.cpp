#include "Graphics.h"
#include "D3D12Helper.h"
#include "Assets.h"

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

		// -- Renderer ---
		void FrameStart();
		void FrameEnd();
		void FrameStart()
		{
			D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();
			// Reset allocator associated with the current buffer
			// and set up the command list to use that allocator
			Graphics::commandAllocators[Graphics::currentSwapBuffer]->Reset();
			Graphics::commandList->Reset(Graphics::commandAllocators[Graphics::currentSwapBuffer].Get(), 0);

			// Grab the current back buffer for this frame
			Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = Graphics::backBuffers[Graphics::currentSwapBuffer];
			// Clearing the render target
			{
				// Transition the back buffer from present to render target
				D3D12_RESOURCE_BARRIER rb = {};
				rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				rb.Transition.pResource = currentBackBuffer.Get();
				rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
				rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				Graphics::commandList->ResourceBarrier(1, &rb);
				// Background color (Cornflower Blue in this case) for clearing
				//float color[] = { 0.4f, 0.6f, 0.75f, 1.0f };
				float color[] = { 0.1f, 0.15f, 0.1875f, 1.0f };
				// Clear the RTV
				Graphics::commandList->ClearRenderTargetView(
					Graphics::rtvHandles[Graphics::currentSwapBuffer],
					color,
					0, 0); // No scissor rectangles
				// Clear the depth buffer, too
				Graphics::commandList->ClearDepthStencilView(
					Graphics::dsvHandle,
					D3D12_CLEAR_FLAG_DEPTH,
					1.0f, // Max depth = 1.0f
					0, // Not clearing stencil, but need a value
					0, 0); // No scissor rects
			}
		}
		void FrameEnd()
		{
			D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();
			// Eventually would put UI rendering here

			// Present
			{
				// Transition back to present
				D3D12_RESOURCE_BARRIER rb = {};
				rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				rb.Transition.pResource = backBuffers[currentSwapBuffer].Get();
				rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
				rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				Graphics::commandList->ResourceBarrier(1, &rb);
				// Must occur BEFORE present
				D3D12Helper::GetInstance().ExecuteCommandList();
				// Present the current back buffer
				bool vsyncNecessary = Graphics::VsyncState();
				Graphics::swapChain->Present(
					vsyncNecessary ? 1 : 0,
					vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

				// Figure out which buffer is next
				Graphics::currentSwapBuffer = d3d12Helper.SyncSwapChain(Graphics::currentSwapBuffer);
			}
		}
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

void Graphics::RenderSimple(std::shared_ptr<Scene> scene, 
	unsigned int activeLightCount)
{
	FrameStart();

	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();
	// Set overall pipeline state
	//Graphics::commandList->SetPipelineState(Assets::GetInstance().GetPiplineState(L"PipelineStates/BasicCullNone").Get());
	// Root sig (must happen before root descriptor table)
	//Graphics::commandList->SetGraphicsRootSignature(Assets::GetInstance().GetRootSig(L"RootSigs/BasicRootSig").Get());
	// Set up other commands for rendering
	Graphics::commandList->OMSetRenderTargets(1, &Graphics::rtvHandles[Graphics::currentSwapBuffer], true, &Graphics::dsvHandle);
	Graphics::commandList->RSSetViewports(1, &Graphics::viewport);
	Graphics::commandList->RSSetScissorRects(1, &Graphics::scissorRect);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = d3d12Helper.GetCBVSRVDescriptorHeap();
	Graphics::commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());
	// Draw
	std::vector<std::shared_ptr<Entity>> entities = scene->GetEntities();;
	for (std::shared_ptr<Entity> entityPtr : entities)
	{
		for (int i = 0; i < entityPtr->GetMeshes().size(); i++)
		{
			std::shared_ptr<Material> mat = entityPtr->GetMaterials()[i];
			if (!mat->GetFinalGPUHandleForTextures().ptr ){ continue; }

			Graphics::commandList->SetPipelineState(entityPtr->GetMaterials()[i]->GetPipelineState().Get()); // Set the pipeline state
			Graphics::commandList->SetGraphicsRootSignature(entityPtr->GetMaterials()[i]->GetRootSignature().Get());
			Graphics::commandList->IASetPrimitiveTopology(entityPtr->GetMaterials()[i]->GetTopology());
			// Vertex Data
			{
				// Fill out a VertexShaderExternalData struct with the entity’s world matrix and the camera’s matrices.
				VSPerFrameData vsframedata = {};
				vsframedata.view = scene->GetCurrentCamera()->GetView();
				vsframedata.projection = scene->GetCurrentCamera()->GetProjection();
				// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
				// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
				D3D12_GPU_DESCRIPTOR_HANDLE vsPerFramehandle =
					d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsframedata), sizeof(VSPerFrameData));
				// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
				Graphics::commandList->SetGraphicsRootDescriptorTable(0, vsPerFramehandle);

				// Fill out a VertexShaderExternalData struct with the entity’s world matrix and the camera’s matrices.
				VSPerObjectData vsobjData = {};
				vsobjData.world = entityPtr->GetTransform()->GetWorldMatrix();
				vsobjData.worldInvTranspose = entityPtr->GetTransform()->GetWorldInverseTransposeMatrix();
				// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
				// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
				D3D12_GPU_DESCRIPTOR_HANDLE vsPerObjhandle =
					d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsobjData), sizeof(VSPerObjectData));
				// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
				Graphics::commandList->SetGraphicsRootDescriptorTable(1, vsPerObjhandle);
			}

			// Pixel Shader
			{
				PSPerFrameData psFrameData = {};
				psFrameData.cameraPosition = scene->GetCurrentCamera()->GetTransform()->GetPosition();
				psFrameData.lightCount = (int)scene->GetLights().size();
				memcpy(psFrameData.lights, &scene->GetLights()[0], sizeof(Light) * MAX_LIGHTS);

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE psPerFrameHandle = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psFrameData), sizeof(PSPerFrameData));

				// Set this constant buffer handle
				// Note: This assumes that descriptor table 1 is the
				//       place to put this particular descriptor.  This
				//       is based on how we set up our root signature.
				Graphics::commandList->SetGraphicsRootDescriptorTable(2, psPerFrameHandle);

				PSPerMaterialData psMatData = {};
				psMatData.colorTint = mat->GetColorTint();
				if (mat->GetRoughness() != -1) psMatData.colorTint.w = mat->GetRoughness(); // Store roughness in the alpha of colorTint
				psMatData.uvScale = mat->GetUVScale();
				psMatData.uvOffset = mat->GetUVOffset();
				D3D12_GPU_DESCRIPTOR_HANDLE psPerMatHandle = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psMatData), sizeof(PSPerMaterialData));
				Graphics::commandList->SetGraphicsRootDescriptorTable(3, psPerMatHandle);
			}
			// Set the SRV descriptor handle for this material's textures
			// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
			Graphics::commandList->SetGraphicsRootDescriptorTable(4, mat->GetFinalGPUHandleForTextures());


			// Grab the vertex buffer view and index buffer view from this entity’s mesh
			D3D12_INDEX_BUFFER_VIEW indexBuffView = entityPtr->GetMeshes()[i]->GetIndexBufferView();
			D3D12_VERTEX_BUFFER_VIEW vertexBuffView = entityPtr->GetMeshes()[i]->GetVertexBufferView();
			// Set them using IASetVertexBuffers() and IASetIndexBuffer()
			Graphics::commandList->IASetIndexBuffer(&indexBuffView);
			Graphics::commandList->IASetVertexBuffers(0, 1, &vertexBuffView);

			// Call DrawIndexedInstanced() using the index count of this entity’s mesh
			Graphics::commandList->DrawIndexedInstanced(entityPtr->GetMeshes()[i]->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	//// Render the Sky
	{
		// Set overall pipeline state
		Graphics::commandList->SetPipelineState(Assets::GetInstance().GetPiplineState(L"PipelineStates/Sky").Get());
		// Root sig (must happen before root descriptor table)
		Graphics::commandList->SetGraphicsRootSignature(Assets::GetInstance().GetRootSig(L"RootSigs/Sky").Get());
		// Vertex Data
		{
			// Fill out a SkyVSData struct with the camera’s matrices.
			VSPerFrameData data = {};
			data.view = scene->GetCurrentCamera()->GetView();
			data.projection = scene->GetCurrentCamera()->GetProjection();
			// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
			// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
			D3D12_GPU_DESCRIPTOR_HANDLE handle =
				d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&data), sizeof(VSPerFrameData));
			// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
			Graphics::commandList->SetGraphicsRootDescriptorTable(0, handle);
		}

		// Pixel Shader
		{
			SkyPSData psData = {};
			psData.colorTint = scene->GetSky()->GetColorTint();

			// Send this to a chunk of the constant buffer heap
			// and grab the GPU handle for it so we can set it for this draw
			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psData), sizeof(SkyPSData));

			// Set this constant buffer handle
			// Note: This assumes that descriptor table 1 is the
			//       place to put this particular descriptor.  This
			//       is based on how we set up our root signature.
			Graphics::commandList->SetGraphicsRootDescriptorTable(2, cbHandlePS);
		}
		// Set the SRV descriptor handle for this sky's textures
		// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
		Graphics::commandList->SetGraphicsRootDescriptorTable(4, scene->GetSky()->GetTextureGPUHandle());

		// Grab the vertex buffer view and index buffer view from this entity’s mesh
		D3D12_INDEX_BUFFER_VIEW indexBuffView = scene->GetSky()->GetMesh()->GetIndexBufferView();
		D3D12_VERTEX_BUFFER_VIEW vertexBuffView = scene->GetSky()->GetMesh()->GetVertexBufferView();
		// Set them using IASetVertexBuffers() and IASetIndexBuffer()
		Graphics::commandList->IASetIndexBuffer(&indexBuffView);
		Graphics::commandList->IASetVertexBuffers(0, 1, &vertexBuffView);

		// Call DrawIndexedInstanced() using the index count of this entity’s mesh
		Graphics::commandList->DrawIndexedInstanced(scene->GetSky()->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	}

	FrameEnd();
}


void Graphics::RenderOptimized(std::shared_ptr<Scene> scene, unsigned int activeLightCount)
{
	// Perform Frame Start operations
	FrameStart();

	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();

	// Set up other commands for rendering
	commandList->OMSetRenderTargets(1, &rtvHandles[currentSwapBuffer], true, &dsvHandle);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = d3d12Helper.GetCBVSRVDescriptorHeap();
	commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());

	// Collect all per-frame data and copy to GPU
	// -- VS
	std::shared_ptr<Camera> camera = scene->GetCurrentCamera();
	VSPerFrameData vsPerFrameData = {};
	vsPerFrameData.view = scene->GetCurrentCamera()->GetView();
	vsPerFrameData.projection = scene->GetCurrentCamera()->GetProjection();
	D3D12_GPU_DESCRIPTOR_HANDLE vsPerFramehandle =
		d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsPerFrameData), sizeof(VSPerFrameData));
	// -- PS
	PSPerFrameData psPerFrameData = {};
	psPerFrameData.cameraPosition = scene->GetCurrentCamera()->GetTransform()->GetPosition();
	psPerFrameData.lightCount = (int)scene->GetLights().size();
	DirectX::XMFLOAT3 ambientColor = scene->GetSky()->GetLights()[0].Color;
	const float ambMult = 0.05f;
	psPerFrameData.ambient = DirectX::XMFLOAT4(ambientColor.x * ambMult, ambientColor.y * ambMult, ambientColor.z * ambMult, 1);
	memcpy(psPerFrameData.lights, &scene->GetLights()[0], sizeof(Light) * MAX_LIGHTS);
	D3D12_GPU_DESCRIPTOR_HANDLE psPerFrameHandle = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
		(void*)(&psPerFrameData), sizeof(PSPerFrameData));
	

	// Get the sorted renderable list
	if (!scene->OpaqueReady()) { scene->InitialSort(); }
	std::vector<std::shared_ptr<Entity>> toDraw(scene->GetOpaqueEntities());

	// Draw all of the entities
	Microsoft::WRL::ComPtr<ID3D12PipelineState> currentPipelineState = 0;
	std::shared_ptr<Material> currentMaterial = 0;
	std::shared_ptr<Mesh> currentMesh = 0;
	for (std::shared_ptr<Entity> entityPtr : toDraw)
	{
		for (int i = 0; i < entityPtr->GetMeshes().size(); i++)
		{
			// Track the current material and swap as necessary
			// (including swapping shaders)
			if (currentMaterial != entityPtr->GetMaterials()[i])
			{
				currentMaterial = entityPtr->GetMaterials()[i];
				// Swap pipeline state if necessary
				if (currentPipelineState != currentMaterial->GetPipelineState())
				{
					currentPipelineState = currentMaterial->GetPipelineState();
					commandList->SetPipelineState(currentPipelineState.Get());
					commandList->SetGraphicsRootSignature(currentMaterial->GetRootSignature().Get());
					commandList->IASetPrimitiveTopology(currentMaterial->GetTopology());

					// Input Per Frame Data
					commandList->SetGraphicsRootDescriptorTable(0, vsPerFramehandle);
					commandList->SetGraphicsRootDescriptorTable(2, psPerFrameHandle);
				}

				// Set Pixel Shader Data
				PSPerMaterialData psData = {};
				psData.colorTint = currentMaterial->GetColorTint();
				if (currentMaterial->GetRoughness() != -1) psData.colorTint.w = currentMaterial->GetRoughness(); // Store roughness in the alpha of colorTint
				psData.uvScale = currentMaterial->GetUVScale();
				psData.uvOffset = currentMaterial->GetUVOffset();
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psData), sizeof(PSPerMaterialData));

				commandList->SetGraphicsRootDescriptorTable(3, cbHandlePS);

				// Set the SRV descriptor handle for this material's textures
				// Note: This assumes that descriptor table 4 is for textures (as per our root sig)
				commandList->SetGraphicsRootDescriptorTable(4, currentMaterial->GetFinalGPUHandleForTextures());
			}

			// Also track current mesh
			if (currentMesh != entityPtr->GetMeshes()[i])
			{
				currentMesh = entityPtr->GetMeshes()[i];

				// Grab the vertex buffer view and index buffer view from this entity’s mesh
				D3D12_INDEX_BUFFER_VIEW indexBuffView = currentMesh->GetIndexBufferView();
				D3D12_VERTEX_BUFFER_VIEW vertexBuffView = currentMesh->GetVertexBufferView();
				// Set them using IASetVertexBuffers() and IASetIndexBuffer()
				commandList->IASetIndexBuffer(&indexBuffView);
				commandList->IASetVertexBuffers(0, 1, &vertexBuffView);
			}

			// Per Object Data (Only Vertex right now)
			{
				VSPerObjectData vsData = {};
				vsData.world = entityPtr->GetTransform()->GetWorldMatrix();
				vsData.worldInvTranspose = entityPtr->GetTransform()->GetWorldInverseTransposeMatrix();
				D3D12_GPU_DESCRIPTOR_HANDLE handle =
					d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsData), sizeof(VSPerObjectData));
				commandList->SetGraphicsRootDescriptorTable(1, handle);
			}

			// Call DrawIndexedInstanced() using the index count of this entity’s mesh
			commandList->DrawIndexedInstanced(currentMesh->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	//// Render the Sky
	{
		// Set overall pipeline state
		Graphics::commandList->SetPipelineState(Assets::GetInstance().GetPiplineState(L"PipelineStates/Sky").Get());
		// Root sig (must happen before root descriptor table)
		Graphics::commandList->SetGraphicsRootSignature(Assets::GetInstance().GetRootSig(L"RootSigs/Sky").Get());
		// Vertex Data
		{
			// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
			// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
			D3D12_GPU_DESCRIPTOR_HANDLE handle =
				d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsPerFrameData), sizeof(VSPerFrameData));
			// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
			Graphics::commandList->SetGraphicsRootDescriptorTable(0, handle);
		}

		// Pixel Shader
		{
			SkyPSData psData = {};
			psData.colorTint = scene->GetSky()->GetColorTint();

			// Send this to a chunk of the constant buffer heap
			// and grab the GPU handle for it so we can set it for this draw
			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psData), sizeof(SkyPSData));

			// Set this constant buffer handle
			// Note: This assumes that descriptor table 1 is the
			//       place to put this particular descriptor.  This
			//       is based on how we set up our root signature.
			Graphics::commandList->SetGraphicsRootDescriptorTable(2, cbHandlePS);
		}
		// Set the SRV descriptor handle for this sky's textures
		// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
		Graphics::commandList->SetGraphicsRootDescriptorTable(4, scene->GetSky()->GetTextureGPUHandle());

		// Grab the vertex buffer view and index buffer view from this entity’s mesh
		D3D12_INDEX_BUFFER_VIEW indexBuffView = scene->GetSky()->GetMesh()->GetIndexBufferView();
		D3D12_VERTEX_BUFFER_VIEW vertexBuffView = scene->GetSky()->GetMesh()->GetVertexBufferView();
		// Set them using IASetVertexBuffers() and IASetIndexBuffer()
		Graphics::commandList->IASetIndexBuffer(&indexBuffView);
		Graphics::commandList->IASetVertexBuffers(0, 1, &vertexBuffView);

		// Call DrawIndexedInstanced() using the index count of this entity’s mesh
		Graphics::commandList->DrawIndexedInstanced(scene->GetSky()->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
	}

	// Perform Frame End operations
	FrameEnd();
}
