#include "D3D12Helper.h"
#include "WICTextureLoader.h"
#include "ResourceUploadBatch.h"
#include <DDSTextureLoader.h>
using namespace DirectX;


// Singleton requirement
D3D12Helper* D3D12Helper::instance;

D3D12Helper::~D3D12Helper() 
{
	WaitForGPU();
	delete[] frameSyncFenceCounters;
}

// --------------------------------------------------------
// Sets up the helper with required DX12 objects
// --------------------------------------------------------
void D3D12Helper::Initialize(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>* commandAllocators,
	unsigned int numBackBuffers)
{
	// Save objects
	this->device = device;
	this->commandList = commandList;
	this->commandQueue = commandQueue;
	this->commandAllocators = commandAllocators;
	this->numBackBuffers = numBackBuffers;
	// Create the fence for basic synchronization
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(waitFence.GetAddressOf()));
	waitFenceEvent = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
	waitFenceCounter = 0;
	// Create the fence for frame sync
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(frameSyncFence.GetAddressOf()));
	frameSyncFenceEvent = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
	frameSyncFenceCounters = new UINT64[numBackBuffers];
	ZeroMemory(frameSyncFenceCounters, sizeof(UINT64) * numBackBuffers);

	// Create heaps for buffer wrangling
	CreateConstantBufferUploadHeap();
	CreateCBVSRVDescriptorHeap();
	CreateImGuiHeap();
}
// --------------------------------------------------------
// Closes the current command list and tells the GPU to
// start executing those commands.  This will NOT wait
// for the GPU or reset the command list!  The caller
// must do those if necessary.
// --------------------------------------------------------
void D3D12Helper::ExecuteCommandList()
{
	// Close the current list and execute it as our only list
	commandList->Close();
	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists);
}
// --------------------------------------------------------
// Makes our C++ code wait for the GPU to finish its
// current batch of work before moving on.
// --------------------------------------------------------
void D3D12Helper::WaitForGPU()
{
	// Update our ongoing fence value (a unique index for each "stop sign")
	// and then place that value into the GPU's command queue
	waitFenceCounter++;
	commandQueue->Signal(waitFence.Get(), waitFenceCounter);
	// Check to see if the most recently completed fence value
	// is less than the one we just set.
	if (waitFence->GetCompletedValue() < waitFenceCounter)
	{
		// Tell the fence to let us know when it's hit, and then
		// sit an wait until that fence is hit.
		waitFence->SetEventOnCompletion(waitFenceCounter, waitFenceEvent);
		WaitForSingleObject(waitFenceEvent, INFINITE);
	}
	ResetFrameSyncCounters();
}

void D3D12Helper::ResetFrameSyncCounters()
{
	ZeroMemory(frameSyncFenceCounters, sizeof(UINT64) * numBackBuffers);
	for (int i = 0; i < (int)numBackBuffers; i++)
	{
		SyncSwapChain(i);
	}
}

// --------------------------------------------------------
// Adds a signal to the command queue so we can track
// which frames have been completed.
// --------------------------------------------------------
unsigned int D3D12Helper::SyncSwapChain(unsigned int currentSwapBufferIndex)
{
	// Grab the current fence value so we can adjust it for the next frame,
	// but first use it to signal this frame being done
	UINT64 currentFenceCounter = frameSyncFenceCounters[currentSwapBufferIndex];
	commandQueue->Signal(frameSyncFence.Get(), currentFenceCounter);

	// Calculate the next index
	unsigned int nextBuffer = currentSwapBufferIndex + 1;
	nextBuffer %= numBackBuffers;

	// Do we need to wait for the next frame?  We do this by checking the counter
	// associated with that frame's buffer and waiting if it's not complete
	if (frameSyncFence->GetCompletedValue() < frameSyncFenceCounters[nextBuffer])
	{
		// Not completed, so we wait
		frameSyncFence->SetEventOnCompletion(frameSyncFenceCounters[nextBuffer], frameSyncFenceEvent);
		WaitForSingleObject(frameSyncFenceEvent, INFINITE);
	}

	// Frame is done, so update the next frame's counter
	frameSyncFenceCounters[nextBuffer] = currentFenceCounter + 1;

	// Return the new buffer index, which the caller can
	// use to track which buffer to use for the next frame
	return nextBuffer;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Helper::LoadTexture(const wchar_t* file, bool generateMips)
{
	// Helper function from DXTK for uploading a resource
	// (like a texture) to the appropriate GPU memory
	ResourceUploadBatch upload(device.Get());
	upload.Begin();

	// Attempt to create the texture
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	CreateWICTextureFromFile(device.Get(), upload, file, texture.GetAddressOf(), generateMips);

	// Perform the upload and wait for it to finish before returning the texture
	auto finish = upload.End(commandQueue.Get());
	finish.wait();

	// Now that we have the texture, add to our list and make a CPU-side descriptor heap
	// just for this texture's SRV.  Note that it would probably be better to put all 
	// texture SRVs into the same descriptor heap, but we don't know how many we'll need
	// until they're all loaded and this is a quick and dirty implementation!
	textures.push_back(texture);

	// Create the CPU-SIDE descriptor heap for our descriptor
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Non-shader visible for CPU-side-only descriptor heap!
	dhDesc.NodeMask = 0;
	dhDesc.NumDescriptors = 1;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(descHeap.GetAddressOf()));
	cpuSideTextureDescriptorHeaps.push_back(descHeap);

	// Create the SRV on this descriptor heap
	// Note: Using a null description results in the "default" SRV (same format, all mips, all array slices, etc.)
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(texture.Get(), 0, cpuHandle);

	// Return the CPU descriptor handle, which can be used to
	// copy the descriptor to a shader-visible heap later
	return cpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Helper::LoadTextureDDS(const wchar_t* file, bool generateMips, bool isCubeMap)
{
	// Helper function from DXTK for uploading a resource
	// (like a texture) to the appropriate GPU memory
	ResourceUploadBatch upload(device.Get());
	upload.Begin();

	// Attempt to create the texture
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	CreateDDSTextureFromFile(device.Get(), upload, file, texture.GetAddressOf(), generateMips, 0Ui64, 0,
		&isCubeMap);

	// Perform the upload and wait for it to finish before returning the texture
	auto finish = upload.End(commandQueue.Get());
	finish.wait();

	// Now that we have the texture, add to our list and make a CPU-side descriptor heap
	// just for this texture's SRV.  Note that it would probably be better to put all 
	// texture SRVs into the same descriptor heap, but we don't know how many we'll need
	// until they're all loaded and this is a quick and dirty implementation!
	textures.push_back(texture);

	// Create the CPU-SIDE descriptor heap for our descriptor
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Non-shader visible for CPU-side-only descriptor heap!
	dhDesc.NodeMask = 0;
	dhDesc.NumDescriptors = 1;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(descHeap.GetAddressOf()));
	cpuSideTextureDescriptorHeaps.push_back(descHeap);

	// Create the SRV on this descriptor heap
	// Create a Shader Resource View Description
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texture.Get()->GetDesc().Format; // Same format as texture
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MostDetailedMip = 0; // Index of the first mip we want to see
	srvDesc.TextureCube.MipLevels = 1; // Only need access to 1 mip
	srvDesc.TextureCube.ResourceMinLODClamp = 0; // Minimum mipmap level that can be accessed (0 means all)
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = 6;
	srvDesc.Texture2DArray.PlaneSlice = 0;

	// Note: Using a null description results in the "default" SRV (same format, all mips, all array slices, etc.)
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(texture.Get(), isCubeMap ? &srvDesc : 0, cpuHandle);

	// Return the CPU descriptor handle, which can be used to
	// copy the descriptor to a shader-visible heap later
	return cpuHandle;
}

// --------------------------------------------------------
// Copy one or more CPU-side (non-shader-visible) descriptors 
// to the final descriptor heap that we use when drawing(the CBV / SRV descriptor heap) 
// and return the handle to the location of the first one
// --------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE D3D12Helper::CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE firstDescriptorToCopy, unsigned int numDescriptorsToCopy)
{
	// Grab the actual heap start on both sides and offset to the next open SRV portion
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
		cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
		cbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += (SIZE_T)srvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
	gpuHandle.ptr += (SIZE_T)srvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
	// We know where to copy these descriptors, so copy all of them and remember the new offset
	device->CopyDescriptorsSimple(
		numDescriptorsToCopy,
		cpuHandle,
		firstDescriptorToCopy,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	srvDescriptorOffset += numDescriptorsToCopy;
	// Pass back the GPU handle to the start of this section
	// in the final CBV/SRV heap so the caller can use it later
	return gpuHandle;

}

// --------------------------------------------------------
// Creates a single CB upload heap which will store all
// constant buffer data for the entire program. This
// heap is treated as a ring buffer, allowing the program
// to continually re-use the memory as frames progress.
// --------------------------------------------------------
void D3D12Helper::CreateConstantBufferUploadHeap()
{
	// This heap MUST have a size that is a multiple of 256
	// We'll support up to the max number of CBs if they're
	// all 256 bytes or less, or fewer overall CBs if they're larger
	cbUploadHeapSizeInBytes = maxConstantBuffers * 256;
	// Assume the first CB will start at the beginning of the heap
	// This offset changes as we use more CBs, and wraps around when full
	cbUploadHeapOffsetInBytes = 0;
	// Create the upload heap for our constant buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Upload heap since we'll be copying often!
	heapProps.VisibleNodeMask = 1;
	// Fill out description
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Height = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Width = cbUploadHeapSizeInBytes; // Must be 256 byte aligned!
	// Create a constant buffer resource heap
	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(cbUploadHeap.GetAddressOf()));
	// Keep mapped!
	D3D12_RANGE range{ 0, 0 };
	cbUploadHeap->Map(0, &range, &cbUploadHeapStartAddress);
}

// --------------------------------------------------------
// Creates a single CBV descriptor heap which will store all
// CBVs and SRVs for the entire program. Like the CBV upload heap,
// this heap is treated as a ring buffer, allowing the program
// to continually re-use the memory as frames progress.
// --------------------------------------------------------
void D3D12Helper::CreateCBVSRVDescriptorHeap()
{
	// Ask the device for the increment size for CBV descriptor heaps
	// This can vary by GPU so we need to query for it
	cbvSrvDescriptorHeapIncrementSize =
		(SIZE_T)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// Describe the descriptor heap we want to make
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Shaders can see these!
	dhDesc.NodeMask = 0; // Node here means physical GPU - we only have 1 so its index is 0
	dhDesc.NumDescriptors = maxConstantBuffers + maxTextureDescriptors; // How many descriptors will we need?
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // This heap can store CBVs, SRVs and UAVs
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(cbvSrvDescriptorHeap.GetAddressOf()));
	// Assume the first CBV will be at the beginning of the heap
	// This will increase as we use more CBVs and will wrap back to 0
	cbvDescriptorOffset = 0;
	// Assume the first SRV will be after all possible CBVs
	srvDescriptorOffset = maxConstantBuffers;
}

void D3D12Helper::CreateImGuiHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC DescriptorImGuiRender = {};
	DescriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	DescriptorImGuiRender.NumDescriptors = 3;
	DescriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device->CreateDescriptorHeap(&DescriptorImGuiRender, IID_PPV_ARGS(imGuiHeap.GetAddressOf()));
}


// --------------------------------------------------------
// Helper for creating a static buffer that will get
// data once and remain immutable
//
// dataStride - The size of one piece of data in the buffer (like a vertex)
// dataCount - How many pieces of data (like how many vertices)
// data - Pointer to the data itself
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D12Resource> D3D12Helper::CreateStaticBuffer(
	unsigned int dataStride, unsigned int dataCount, void* data)
{
	// Creates a temporary command allocator and list so we don't
	// screw up any other ongoing work (since resetting a command allocator
	// cannot happen while its list is being executed).  These ComPtrs will
	// be cleaned up automatically when they go out of scope.
	// Note: This certainly isn't efficient, but hopefully this only
	//       happens during start-up.  Otherwise, refactor this to use
	//       the existing list and allocator(s).
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> localAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> localList;

	device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(localAllocator.GetAddressOf()));

	device->CreateCommandList(
		0,								// Which physical GPU will handle these tasks?  0 for single GPU setup
		D3D12_COMMAND_LIST_TYPE_DIRECT,	// Type of command list - direct is for standard API calls
		localAllocator.Get(),			// The allocator for this list (to start)
		0,								// Initial pipeline state - none for now
		IID_PPV_ARGS(localList.GetAddressOf()));

	// The overall buffer we'll be creating
	Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
	// Describes the final heap
	D3D12_HEAP_PROPERTIES props = {};
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.CreationNodeMask = 1;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	props.Type = D3D12_HEAP_TYPE_DEFAULT;
	props.VisibleNodeMask = 1;
	D3D12_RESOURCE_DESC desc = {};
	desc.Alignment = 0;
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Height = 1; // Assuming this is a regular buffer, not a texture
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Width = dataStride * dataCount; // Size of the buffer
	device->CreateCommittedResource(
		&props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST, // Will eventually be "common", but we're copying first
		0,
		IID_PPV_ARGS(buffer.GetAddressOf()));
	// Now create an intermediate upload heap for copying initial data
	D3D12_HEAP_PROPERTIES uploadProps = {};
	uploadProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadProps.CreationNodeMask = 1;
	uploadProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Can only ever be Generic_Read state
	uploadProps.VisibleNodeMask = 1;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap;
	device->CreateCommittedResource(
		&uploadProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(uploadHeap.GetAddressOf()));
	// Do a straight map/memcpy/unmap
	void* gpuAddress = 0;
	uploadHeap->Map(0, 0, &gpuAddress);
	memcpy(gpuAddress, data, dataStride * dataCount);
	uploadHeap->Unmap(0, 0);
	// Copy the whole buffer from uploadheap to vert buffer
	localList->CopyResource(buffer.Get(), uploadHeap.Get());
	// Transition the buffer to generic read for the rest of the app lifetime (presumable)
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = buffer.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	localList->ResourceBarrier(1, &rb);
	// Execute the command list and return the finished buffer
	// Execute the local command list and wait for it to complete
	// before returning the final buffer
	localList->Close();
	ID3D12CommandList* list[] = { localList.Get() };
	commandQueue->ExecuteCommandLists(1, list);

	WaitForGPU();
	return buffer;
}

// Used the following page to help format a Structured Buffer
// https://www.stefanpijnacker.nl/article/directx12-resources-key-concepts/
D3D12_CPU_DESCRIPTOR_HANDLE D3D12Helper::CreateParticleBuffer(unsigned long long sizeOfParticle, int maxParticles,
	Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer)
{
	// Make a dynamic buffer to hold all particle data on GPU (ID312Resource)
	// Note: We'll be overwriting this every frame with new lifetime data
	Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;  // Read-Write Access

	SIZE_T reservationSize = (SIZE_T)(sizeOfParticle * maxParticles);
	reservationSize = (reservationSize + 255) / 256 * 256; // Integer division trick
	resDesc.Width = reservationSize; // Doesn't have to be 256-byte aligned, but why not
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;

	resDesc.Alignment = 0;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;

	D3D12_HEAP_PROPERTIES gpuHeapProp = D3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	device->CreateCommittedResource(
		&gpuHeapProp,
		D3D12_HEAP_FLAG_NONE, 
		&resDesc,
		D3D12_RESOURCE_STATE_COMMON, 
		nullptr, 
		IID_PPV_ARGS(&particleBuffer));

	// Now that we have the texture, add to our list and make a CPU-side descriptor heap
	// just for this texture's SRV.  Note that it would probably be better to put all 
	// texture SRVs into the same descriptor heap, but we don't know how many we'll need
	// until they're all loaded and this is a quick and dirty implementation!
	textures.push_back(particleBuffer);
	outBuffer = particleBuffer;

	// Create the CPU-SIDE descriptor heap for our descriptor
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Non-shader visible for CPU-side-only descriptor heap!
	dhDesc.NodeMask = 0;
	dhDesc.NumDescriptors = 1;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	device->CreateDescriptorHeap(&dhDesc, 
		IID_PPV_ARGS(descHeap.GetAddressOf()));
	cpuSideTextureDescriptorHeaps.push_back(descHeap);

	// Create an SRV that points to a structured buffer of particles
	// so we can grab this data in a vertex shader
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // This description is correct
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	srvDesc.Buffer.StructureByteStride = sizeOfParticle;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// Create the SRV
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(particleBuffer.Get(), &srvDesc, cpuHandle);


	return cpuHandle;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3D12Helper::GetCBVSRVDescriptorHeap()
{
	return cbvSrvDescriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> D3D12Helper::GetImGuiHeap()
{
	return imGuiHeap;
}

// --------------------------------------------------------
// Copies the given data into the next "unused" spot in the CBV upload heap (wrapping at the end, since
// we treat it like a ring buffer). Then creates a CBV in the next "unused" spot in the CBV heap that
// points to the aforementioned spot in the upload heap and returns that CBV (a GPU descriptor handle).
//
// data - The data to copy to the GPU
// dataSizeInBytes - The byte size of the data to copy
// --------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE D3D12Helper::FillNextConstantBufferAndGetGPUDescriptorHandle(
	void* data, unsigned int dataSizeInBytes)
{
	// How much space will we need? Each CBV must point to a chunk of the upload heap that is
	// a multiple of 256 bytes, so we need to calculate and reserve that amount.
	SIZE_T reservationSize = (SIZE_T)dataSizeInBytes;
	reservationSize = (reservationSize + 255) / 256 * 256; // Integer division trick
	// Ensure this upload will fit in the remaining space. If not, reset to beginning.
	if (cbUploadHeapOffsetInBytes + reservationSize >= cbUploadHeapSizeInBytes)
		cbUploadHeapOffsetInBytes = 0;
	// Where in the upload heap will this data go?
	D3D12_GPU_VIRTUAL_ADDRESS virtualGPUAddress = cbUploadHeap->GetGPUVirtualAddress() + cbUploadHeapOffsetInBytes;
	// === Copy data to the upload heap ===
	{
		// Calculate the actual upload address (which we got from mapping the buffer)
		// Note that this is different than the GPU virtual address needed for the CBV below
		void* uploadAddress = reinterpret_cast<void*>(
			(SIZE_T)cbUploadHeapStartAddress + cbUploadHeapOffsetInBytes);
		// Perform the mem copy to put new data into this part of the heap
		memcpy(uploadAddress, data, dataSizeInBytes);
		// Increment the offset and loop back to the beginning if necessary,
		// allowing us to treat the upload heap like a ring buffer
		cbUploadHeapOffsetInBytes += reservationSize;
		if (cbUploadHeapOffsetInBytes >= cbUploadHeapSizeInBytes)
			cbUploadHeapOffsetInBytes = 0;
	}
	// Create a CBV for this section of the heap
	{
		// Calculate the CPU and GPU side handles for this descriptor
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		// Offset each by based on how many descriptors we've used
		// Note: cbvDescriptorOffset is a COUNT of descriptors, not bytes so we must calculate the size
		cpuHandle.ptr += (SIZE_T)cbvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
		gpuHandle.ptr += (SIZE_T)cbvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
		// Describe the constant buffer view that points to our latest chunk of the CB upload heap
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = virtualGPUAddress;
		cbvDesc.SizeInBytes = (UINT)reservationSize;
		// Create the CBV, which is a lightweight operation in DX12
		device->CreateConstantBufferView(&cbvDesc, cpuHandle);
		// Increment the offset and loop back to the beginning if necessary
		// which allows us to treat the descriptor heap as a ring buffer
		cbvDescriptorOffset++;
		if (cbvDescriptorOffset >= maxConstantBuffers)
			cbvDescriptorOffset = 0;
		// Now that the CBV is ready, we return the GPU handle to it
		// so it can be set as part of the root signature during drawing
		return gpuHandle;
	}
}

