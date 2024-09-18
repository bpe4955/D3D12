#include "Assets.h"
//#include "PathHelpers.h"
#include "D3D12Helper.h"

#define _SILENCE_FILESYSTEM_DEPRECATION_WARNING
#include <filesystem>

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <codecvt>
#include <d3dcompiler.h>

#include <fstream>
#include "nlohmann/json.hpp"
#include "Graphics.h"
using json = nlohmann::json;

// Singleton requirement
Assets* Assets::instance;


// --------------------------------------------------------------------------
// Cleans up the asset manager and deletes any resources that are
// not stored with smart pointers.
// --------------------------------------------------------------------------
Assets::~Assets()
{
}

void Assets::Initialize(std::wstring _rootAssetPath, std::wstring _rootShaderPath,
	Microsoft::WRL::ComPtr<ID3D12Device> _device, bool _printLoadingProgress,
	bool _allowOnDemandLoading)
{
	rootAssetPath = _rootAssetPath;
	rootShaderPath = _rootShaderPath;
	device = _device;
	printLoadingProgress = _printLoadingProgress;
	allowOnDemandLoading = _allowOnDemandLoading;

	// Replace all "\\" with "/" to ease lookup later
	std::replace(this->rootAssetPath.begin(), this->rootAssetPath.end(), '\\', '/');
	std::replace(this->rootShaderPath.begin(), this->rootShaderPath.end(), '\\', '/');

	// Ensure the root path ends with a slash
	if (!EndsWith(rootAssetPath, L"/"))
		rootAssetPath += L"/";
	if (!EndsWith(rootShaderPath, L"/"))
		rootShaderPath += L"/";

	if (!allowOnDemandLoading) { LoadAllAssets(); }
}

void Assets::LoadAllAssets()
{
	if (rootAssetPath.empty()) return;
	if (rootShaderPath.empty()) return;

	// These files need to be loaded after all basic assets
	std::vector<std::wstring> pipelinePaths;
	std::vector<std::wstring> materialPaths;
	std::vector<std::wstring> skyPaths;

	// Recursively go through all directories starting at the root
	for (auto& item : std::filesystem::recursive_directory_iterator(FixPath(rootAssetPath)))
	{
		// Is this a regular file?
		if (item.status().type() == std::filesystem::file_type::regular)
		{
			std::wstring itemPath = item.path().wstring();

			// Replace all L"\\" with L"/" to ease lookup later
			std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

			// Determine the file type
			if (EndsWith(itemPath, L".obj") || EndsWith(itemPath, L".fbx") || EndsWith(itemPath, L".dae"))
			{ LoadMesh(itemPath); }
			else if (EndsWith(itemPath, L".jpg") || EndsWith(itemPath, L".png"))
			{ LoadTexture(itemPath); }
			else if (EndsWith(itemPath, L".dds"))
			{ LoadDDSTexture(itemPath, false, true); }
			//else if (EndsWith(itemPath, L".spritefont"))
			//{ LoadSpriteFont(itemPath); }
			else if (EndsWith(itemPath, L".rootsig"))
			{ LoadRootSig(itemPath); }
			else if (EndsWith(itemPath, L".pipeline"))
			{ pipelinePaths.push_back(itemPath); }
			else if (EndsWith(itemPath, L".material"))
			{ materialPaths.push_back(itemPath); }
			else if (EndsWith(itemPath, L".sky"))
			{ skyPaths.push_back(itemPath); }
		}
	}

	// Search and load all shaders in the shader path
	for (auto& item : std::filesystem::directory_iterator(FixPath(rootShaderPath)))
	{
		std::wstring itemPath = item.path().wstring();

		// Replace all L"\\" with L"/" to ease lookup later
		std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

		// Is this a Compiled Shader Object?
		if (EndsWith(itemPath, L".cso"))
		{
			LoadUnknownShader(itemPath);
		}
	}

	// Load all pipelineStates
	for (auto& pPath : pipelinePaths)
	{
		LoadPipelineState(pPath);
	}

	// Load all materials
	for (auto& mPath : materialPaths)
	{
		LoadMaterial(mPath);
	}

	// Load all skies
	for (auto& sPath : skyPaths)
	{
		LoadSky(sPath);
	}
}
//
//	GETTERS
//
// --------------------------------------------------------------------------
// Gets the specified mesh if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the mesh if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
//
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Models/cube"
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::GetMesh(std::wstring name)
{
	// Search and return mesh if found
	auto it = meshes.find(name);
	if (it != meshes.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::wstring filePathOBJ = FixPath(rootAssetPath + name + L".obj");
		if (std::filesystem::exists(filePathOBJ))
		{
			return LoadMesh(filePathOBJ);
		}
		// Is it a FBX?
		std::wstring filePathFBX = FixPath(rootAssetPath + name + L".fbx");
		if (std::filesystem::exists(filePathFBX)) { return LoadMesh(filePathFBX); }
		// Is it a FBX?
		std::wstring filePathDAE = FixPath(rootAssetPath + name + L".dae");
		if (std::filesystem::exists(filePathDAE)) { return LoadMesh(filePathDAE); }
	}

	// Unsuccessful
	return 0;
}
D3D12_CPU_DESCRIPTOR_HANDLE Assets::GetTexture(std::wstring name, bool generateMips, bool isCubeMap)
{
	// Search and return texture if found
	auto it = textures.find(name);
	if (it != textures.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::wstring filePathJPG = FixPath(rootAssetPath + name + L".jpg");
		if (std::filesystem::exists(filePathJPG)) { return LoadTexture(filePathJPG); }

		// Is it a PNG?
		std::wstring filePathPNG = FixPath(rootAssetPath + name + L".png");
		if (std::filesystem::exists(filePathPNG)) { return LoadTexture(filePathPNG); }

		// Is it a DDS?
		std::wstring filePathDDS = FixPath(rootAssetPath + name + L".dds");
		if (std::filesystem::exists(filePathDDS)) { return LoadDDSTexture(filePathDDS, generateMips, isCubeMap); }
	}

	// Unsuccessful
	return {};
}
/// <summary>
/// 
/// </summary>
/// <param name="path">File path to texture folder 
/// Ex: L"Textures/Skies/Planet/"</param>
/// <returns></returns>
Microsoft::WRL::ComPtr<ID3DBlob> Assets::GetPixelShader(std::wstring name)
{
	// Search and return shader if found
	auto it = pixelShaders.find(name);
	if (it != pixelShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(rootShaderPath + name + L".cso");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			Microsoft::WRL::ComPtr<ID3DBlob> ps = LoadPixelShader(filePath);
			return ps;
		}
	}

	// Unsuccessful
	return 0;
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::GetVertexShader(std::wstring name)
{
	// Search and return shader if found
	auto it = vertexShaders.find(name);
	if (it != vertexShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(rootShaderPath + name + L".cso");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			Microsoft::WRL::ComPtr<ID3DBlob> vs = LoadVertexShader(filePath);
			return vs;
		}
	}

	// Unsuccessful
	return 0;
}
Microsoft::WRL::ComPtr<ID3D12RootSignature> Assets::GetRootSig(std::wstring name)
{
	// Search and return shader if found
	auto it = rootSigs.find(name);
	if (it != rootSigs.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(rootAssetPath + name + L".rootsig");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the root signature and return it if successful
			Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig = LoadRootSig(filePath);
			return rootSig;
		}
	}

	// Unsuccessful
	return 0;
}
Microsoft::WRL::ComPtr<ID3D12PipelineState> Assets::GetPiplineState(std::wstring name)
{
	// Search and return shader if found
	auto it = pipelineStates.find(name);
	if (it != pipelineStates.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(rootAssetPath + name + L".pipeline");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the pipeline state and return it if successful
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = LoadPipelineState(filePath);
			return pipelineState;
		}
	}

	// Unsuccessful
	return 0;
}
std::shared_ptr<Material> Assets::GetMaterial(std::wstring name)
{
	// Search and return mesh if found
	auto it = materials.find(name);
	if (it != materials.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".material");
		if (std::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadMaterial(filePath);
		}
	}

	// Unsuccessful
	return 0;
}
std::shared_ptr<Sky> Assets::GetSky(std::wstring name)
{
	// Search and return mesh if found
	auto it = skies.find(name);
	if (it != skies.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		std::wstring filePath = FixPath(rootAssetPath + name + L".sky");
		if (std::filesystem::exists(filePath))
		{
			// Do the load now and return the result
			return LoadSky(filePath);
		}
	}

	// Unsuccessful
	return 0;
}
unsigned int Assets::GetMeshCount() { return (unsigned int)meshes.size(); }
unsigned int Assets::GetTextureCount() { return (unsigned int)textures.size(); }
unsigned int Assets::GetRootSigCount() { return (unsigned int)rootSigs.size(); }
unsigned int Assets::GetPipelineStateCount() { return (unsigned int)pipelineStates.size(); }
unsigned int Assets::GetPixelShaderCount() { return (unsigned int)pixelShaders.size(); }
unsigned int Assets::GetVertexShaderCount() { return (unsigned int)vertexShaders.size(); }
unsigned int Assets::GetMaterialCount() { return (unsigned int)materials.size(); }
unsigned int Assets::GetSkyCount() { return (unsigned int)skies.size(); }
//
//	MODIFIERS
//
void Assets::AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh) { meshes.insert({ name, mesh }); }
void Assets::AddTexture(std::wstring name, D3D12_CPU_DESCRIPTOR_HANDLE texture) { textures.insert({ name, texture }); }
void Assets::AddRootSig(std::wstring name, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig) { rootSigs.insert({ name, rootSig }); }
void Assets::AddPipelineState(std::wstring name, Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState) { pipelineStates.insert({ name, pipelineState }); }
void Assets::AddPixelShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> ps) { pixelShaders.insert({ name, ps }); }
void Assets::AddVertexShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> vs) { vertexShaders.insert({ name, vs }); }
void Assets::AddMaterial(std::wstring name, std::shared_ptr<Material> material) { materials.insert({ name, material }); }
void Assets::AddSky(std::wstring name, std::shared_ptr<Sky> sky) { skies.insert({ name, sky }); }
//
//	PRIVATE LOADING FUNCTIONS
//
std::shared_ptr<Mesh> Assets::LoadMesh(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading mesh: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<Mesh> m = std::make_shared<Mesh>(path.c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	meshes.insert({ filename, m });

	return m;
}

D3D12_CPU_DESCRIPTOR_HANDLE Assets::LoadTexture(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	D3D12_CPU_DESCRIPTOR_HANDLE srv =
		D3D12Helper::GetInstance().LoadTexture(path.c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE Assets::LoadDDSTexture(std::wstring path, bool generateMips, bool isCubeMap)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	D3D12_CPU_DESCRIPTOR_HANDLE srv =
		D3D12Helper::GetInstance().LoadTextureDDS(path.c_str(), generateMips, isCubeMap);

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}

/// <summary>
/// Create a tex cube from textures in a given folder.
/// https://alextardif.com/D3D11To12P3.html
/// </summary>
/// <param name="path">The folder that all the textures are stored in. 
/// Assumes they are named "back", "down", etc. and are pngs.</param>
/// <returns>GPU descriptor handle of the created Texture Cube</returns>
/*
D3D12_GPU_DESCRIPTOR_HANDLE Assets::LoadTexCubeDEPRECATED(std::wstring path)
{
	D3D12Helper::GetInstance().WaitForGPU();

	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture cube: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not SHADER RESOURCE VIEWS!
	// - Explicitly NOT generating mipmaps, as we don't need them for the sky!
	// - Order matters here! +X, -X, +Y, -Y, +Z, -Z
	DirectX::ResourceUploadBatch upload(Graphics::Device.Get());
	upload.Begin();
	Microsoft::WRL::ComPtr<ID3D12Resource> subTextures[6] = {};
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"right.png").c_str(), subTextures[0].GetAddressOf());
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"left.png").c_str(), subTextures[1].GetAddressOf());
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"up.png").c_str(), subTextures[2].GetAddressOf());
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"down.png").c_str(), subTextures[3].GetAddressOf());
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"front.png").c_str(), subTextures[4].GetAddressOf());
	DirectX::CreateWICTextureFromFile(Graphics::Device.Get(), upload, (path + L"back.png").c_str(), subTextures[5].GetAddressOf());
	// Perform the upload and wait for it to finish before returning the texture
	auto finish = upload.End(Graphics::commandQueue.Get());
	finish.wait();

	// We'll assume all of the textures are the same color format and resolution,
	// so get the description of the first texture
	D3D12_RESOURCE_DESC faceDesc = subTextures[0]->GetDesc();

	// Describes the final heap (assuming it's the same as other textures)
	D3D12_HEAP_PROPERTIES props = {};
	subTextures[0].Get()->GetHeapProperties(&props, 0);
	// Describe the resource for the cube map, which is simply a "texture 2d array"
	D3D12_RESOURCE_DESC cubeDesc = {};
	cubeDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	cubeDesc.Alignment = 0;
	cubeDesc.Width = faceDesc.Width; // Match the size
	cubeDesc.Height = faceDesc.Height; // Match the size
	cubeDesc.DepthOrArraySize = 6; // Cube map!
	cubeDesc.MipLevels = 1; // Only need 1
	cubeDesc.Format = faceDesc.Format; // Match the loaded texture's color format
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the final texture resource to hold the cube map
	Microsoft::WRL::ComPtr<ID3D12Resource> cubeMapTexture;

	// Now create an intermediate upload heap for copying initial data
	D3D12_HEAP_PROPERTIES heapProps = {};
	subTextures[0].Get()->GetHeapProperties(&heapProps, 0);
	D3D12_HEAP_PROPERTIES defaultProperties;
	defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultProperties.CreationNodeMask = 0;
	defaultProperties.VisibleNodeMask = 0;

	Graphics::Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cubeDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(cubeMapTexture.GetAddressOf()));

	// Creates a temporary command allocator and list so we don't
	// screw up any other ongoing work (since resetting a command allocator
	// cannot happen while its list is being executed).  These ComPtrs will
	// be cleaned up automatically when they go out of scope.
	// Note: This certainly isn't efficient, but hopefully this only
	//       happens during start-up.  Otherwise, refactor this to use
	//       the existing list and allocator(s).
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> localAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> localList;

	Graphics::Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(localAllocator.GetAddressOf()));

	Graphics::Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		localAllocator.Get(),
		0,
		IID_PPV_ARGS(localList.GetAddressOf()));

	// Get textures ready to copy
	for (int i = 0; i < 6; i++)
	{
		// Transition the buffer to generic read for the rest of the app lifetime (presumable)
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = subTextures[i].Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		localList->ResourceBarrier(1, &rb);
	}

	// Copy data
	for (UINT subResourceIndex = 0; subResourceIndex < 6; subResourceIndex++)
	{
		D3D12_TEXTURE_COPY_LOCATION destination = {};
		destination.pResource = cubeMapTexture.Get();
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destination.SubresourceIndex = subResourceIndex;

		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = subTextures[subResourceIndex].Get();
		source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		source.SubresourceIndex = 0;

		localList->CopyTextureRegion(&destination, 0, 0, 0, &source, NULL);
	}

	// Transition the texture to shader resource for the rest of the app lifetime (presumable)
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = cubeMapTexture.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	localList->ResourceBarrier(1, &rb);

	// Do the same for all the textures
	for (int i = 0; i < 6; i++)
	{
		// Transition the buffer to generic read for the rest of the app lifetime (presumable)
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = subTextures[i].Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		localList->ResourceBarrier(1, &rb);
	}

	// Execute the command list and return the finished buffer
	// Execute the local command list and wait for it to complete
	// before returning the final buffer
	localList->Close();
	ID3D12CommandList* list[] = { localList.Get() };
	Graphics::commandQueue->ExecuteCommandLists(1, list);
	D3D12Helper::GetInstance().WaitForGPU();
	
	// Store textures so they don't fall out of scope
	for (int i = 0; i < 6; i++)
	{
		//storedTextures.push_back(subTextures[i]);
	}

	// Create a Shader Resource View Description
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format; // Same format as texture
	//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MostDetailedMip = 0; // Index of the first mip we want to see
	srvDesc.TextureCube.MipLevels = 1; // Only need access to 1 mip
	srvDesc.TextureCube.ResourceMinLODClamp = 0; // Minimum mipmap level that can be accessed (0 means all)
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = 6;
	srvDesc.Texture2DArray.PlaneSlice = 0;

	
	// Create the CPU-SIDE descriptor heap for our descriptor
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Non-shader visible for CPU-side-only descriptor heap!
	dhDesc.NodeMask = 0;
	dhDesc.NumDescriptors = 1;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(descHeap.GetAddressOf()));
	//cpuSideTextureDescriptorHeaps.push_back(descHeap);

	// Create the SRV on this descriptor heap
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	Graphics::Device->CreateShaderResourceView(cubeMapTexture.Get(), &srvDesc, cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = D3D12Helper::GetInstance().CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(cpuHandle, 1);
	// Return the CPU descriptor handle, which can be used to
	// copy the descriptor to a shader-visible heap later
	//texCubes.insert({ filename, gpuHandle });
	return gpuHandle;
}
*/

//std::shared_ptr<DirectX::SpriteFont> Assets::LoadSpriteFont(std::wstring path)
//{
//	return std::shared_ptr<DirectX::SpriteFont>();
//}
//
//void Assets::LoadSampler(std::wstring path)
//{
//}

Microsoft::WRL::ComPtr<ID3D12RootSignature> Assets::LoadRootSig(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading root signature: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	if (d.is_discarded())
	{
		// Set a null root signature
		rootSigs.insert({ filename, 0 });
		return 0;
	}

	// Root Signature
	int numVertBuffDesc = 1;
	int numPixBuffDesc = 1;
	if (d.contains("numVertBuffDesc")) { numVertBuffDesc = d["numVertBuffDesc"].get<unsigned int>(); }
	if (d.contains("numPixBuffDesc")) { numVertBuffDesc = d["numPixBuffDesc"].get<unsigned int>(); }

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	// Describe the range of CBVs needed for the vertex shader PER FRAME
	D3D12_DESCRIPTOR_RANGE cbvRangeVSFrame = {};
	cbvRangeVSFrame.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRangeVSFrame.NumDescriptors = 1;
	cbvRangeVSFrame.BaseShaderRegister = 0;
	cbvRangeVSFrame.RegisterSpace = 0;
	cbvRangeVSFrame.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// Describe the range of CBVs needed for the vertex shader PER OBJECT
	D3D12_DESCRIPTOR_RANGE cbvRangeVSObj = {};
	cbvRangeVSObj.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRangeVSObj.NumDescriptors = numVertBuffDesc;
	cbvRangeVSObj.BaseShaderRegister = 1;
	cbvRangeVSObj.RegisterSpace = 0;
	cbvRangeVSObj.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// Describe the range of CBVs needed for the pixel shader PER FRAME
	D3D12_DESCRIPTOR_RANGE cbvRangePSFrame = {};
	cbvRangePSFrame.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRangePSFrame.NumDescriptors = 1;
	cbvRangePSFrame.BaseShaderRegister = 0;
	cbvRangePSFrame.RegisterSpace = 0;
	cbvRangePSFrame.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// Describe the range of CBVs needed for the pixel shader PER MATERIAL
	D3D12_DESCRIPTOR_RANGE cbvRangePSMat = {};
	cbvRangePSMat.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRangePSMat.NumDescriptors = numPixBuffDesc;
	cbvRangePSMat.BaseShaderRegister = 1;
	cbvRangePSMat.RegisterSpace = 0;
	cbvRangePSMat.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	UINT numDescriptors = 4;
	UINT baseShaderRegister = 0;
	if (d.contains("numTextures")) { numDescriptors = d["numTextures"].get<unsigned int>(); }
	if (d.contains("baseShaderRegister")) { baseShaderRegister = d["baseShaderRegister"].get<unsigned int>(); }
	// Create a range of SRV's for textures
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = numDescriptors; // Set to max number of textures at once (match pixel shader!)
	srvRange.BaseShaderRegister = baseShaderRegister; // Starts at s0 (match pixel shader!)
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Create the root parameters
	D3D12_ROOT_PARAMETER rootParams[5] = {};
	// CBV table param for vertex shader
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVSFrame;
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangeVSObj;
	// CBV table param for pixel shader
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &cbvRangePSFrame;
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRangePSMat;
	// SRV table param
	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[4].DescriptorTable.pDescriptorRanges = &srvRange;

	// Create a single static sampler (available to all pixel shaders at the same slot)
	D3D12_TEXTURE_ADDRESS_MODE u = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_TEXTURE_ADDRESS_MODE v = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_TEXTURE_ADDRESS_MODE w = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;
	D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	D3D12_COMPARISON_FUNC compFunc = D3D12_COMPARISON_FUNC_NEVER;
	UINT maxAnisotropy = 16;
	// WRAP   - 1  |  MIRROR      - 2  |  CLAMP - 3
	// BORDER - 4  |  MORROR_ONCE - 5  
	if (d.contains("sampler"))
	{
		if (d["sampler"].contains("u") && d["sampler"]["u"].is_number_integer())
		{
			u = d["sampler"]["u"].get<D3D12_TEXTURE_ADDRESS_MODE>();
		}
		if (d["sampler"].contains("v") && d["sampler"]["v"].is_number_integer())
		{
			v = d["sampler"]["v"].get<D3D12_TEXTURE_ADDRESS_MODE>();
		}
		if (d["sampler"].contains("w") && d["sampler"]["w"].is_number_integer())
		{
			w = d["sampler"]["w"].get<D3D12_TEXTURE_ADDRESS_MODE>();
		}
		if (d["sampler"].contains("filter") && d["sampler"]["filter"].is_string())
		{
			//d["sampler"]["filter"].get<std::string>()
			auto input = d["sampler"]["filter"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "ANISOTROPIC")
			{ filter = D3D12_FILTER_ANISOTROPIC; }
			else if (input == "COMPARISON_ANISOTROPIC" || input == "COMPARISONANISOTROPIC")
			{ filter = D3D12_FILTER_COMPARISON_ANISOTROPIC; }
			else if (input == "MIN_MAG_MIP_LINEAR" || input == "LINEAR")
			{ filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; }
			else if (input == "MIN_MAG_MIP_POINT" || input == "POINT")
			{ filter = D3D12_FILTER_MIN_MAG_MIP_POINT; }
			else if (input == "COMPARISON_MIN_MAG_MIP_LINEAR" || input == "COMPARISONANLINEAR")
			{ filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; }
			else if (input == "COMPARISON_MIN_MAG_MIP_POINT" || input == "COMPARISONANPOINT")
			{ filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT; }
			else if (input == "MINIMUM_MIN_MAG_MIP_LINEAR")
			{ filter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR; }
			else if (input == "MINIMUM_MIN_MAG_MIP_POINT")
			{ filter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT; }
			else if (input == "MAXIMUM_MIN_MAG_MIP_LINEAR")
			{ filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR; }
			else if (input == "MAXIMUM_MIN_MAG_MIP_POINT")
			{ filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT; }
		}
		if (d["sampler"].contains("borderColor") && d["sampler"]["borderColor"].is_string())
		{
			//d["sampler"]["filter"].get<std::string>()
			auto input = d["sampler"]["borderColor"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "TRANSPARENT_BLACK" || input == "TRANSPARENT")
			{ borderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; }
			else if (input == "OPAQUE_BLACK" || input == "BLACK")
			{ borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK; }
			else if (input == "OPAQUE_WHITE" || input == "WHITE")
			{ borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; }
		}
		if (d["sampler"].contains("comparison") && d["sampler"]["comparison"].is_string())
		{
			//d["sampler"]["filter"].get<std::string>()
			auto input = d["sampler"]["comparison"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "NEVER") { compFunc = D3D12_COMPARISON_FUNC_NEVER; }
			else if (input == "LESS") { compFunc = D3D12_COMPARISON_FUNC_LESS; }
			else if (input == "EQUAL") { compFunc = D3D12_COMPARISON_FUNC_EQUAL; }
			else if (input == "LESS_EQUAL" || input == "LESSEQUAL") { compFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; }
			else if (input == "GREATER") { compFunc = D3D12_COMPARISON_FUNC_GREATER; }
			else if (input == "NOT_EQUAL" || input == "NOTEQUAL") { compFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL; }
			else if (input == "GREATER_EQUAL" || input == "GREATEREQUAL") { compFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; }
			else if (input == "ALWAYS") { compFunc = D3D12_COMPARISON_FUNC_ALWAYS; }
		}
		if (d["sampler"].contains("maxAnisotropy") && d["sampler"]["maxAnisotropy"].is_number_integer())
		{
			maxAnisotropy = d["sampler"]["maxAnisotropy"].get<unsigned int>();
		}
	}


	D3D12_STATIC_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = u;
	sampDesc.AddressV = v;
	sampDesc.AddressW = w;
	sampDesc.Filter = filter;
	sampDesc.MaxAnisotropy = maxAnisotropy;
	sampDesc.BorderColor = borderColor;
	sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
	sampDesc.ShaderRegister = 0; // register(s0)
	sampDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampDesc.ComparisonFunc = compFunc;

	D3D12_STATIC_SAMPLER_DESC samplers[] = { sampDesc };
	// Describe the root signature
	D3D12_ROOT_SIGNATURE_DESC rootSig = {};
	rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSig.NumParameters = ARRAYSIZE(rootParams);
	rootSig.pParameters = rootParams;
	rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
	rootSig.pStaticSamplers = samplers;

	// Serialze the root signature
	ID3DBlob* serializedRootSig = 0;
	ID3DBlob* errors = 0;
	D3D12SerializeRootSignature(
		&rootSig,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig,
		&errors);

	// Check for errors during serialization
	if (errors != 0)
	{
		OutputDebugString((wchar_t*)errors->GetBufferPointer());
	}


	// Actually create the root sig
	Graphics::Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf()));

	rootSigs.insert({ filename, rootSignature });

	return rootSignature;
}
Microsoft::WRL::ComPtr<ID3D12PipelineState> Assets::LoadPipelineState(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading pipeline state: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Verify required members (shaders for now)
	if (d.is_discarded() ||
		!d.contains("shaders") ||
		!d["shaders"].contains("pixel") ||
		!d["shaders"].contains("vertex") ||
		!d.contains("rootSig"))
	{
		// Set a null root signature
		rootSigs.insert({ filename, 0 });
		return 0;
	}

	// Load Shaders
	std::wstring vsName = NarrowToWide(d["shaders"]["vertex"].get<std::string>());
	std::wstring psName = NarrowToWide(d["shaders"]["pixel"].get<std::string>());
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode = GetVertexShader(vsName);
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode = GetPixelShader(psName);

	// Load Root Sig
	std::wstring rootSigName = NarrowToWide(d["rootSig"].get<std::string>());
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = GetRootSig(rootSigName);

	// Input layout - Always the same
	if( !inputElementsCreated )
	{
		// Create an input layout that describes the vertex format
		// used by the vertex shader we're using
		//  - This is used by the pipeline to know how to interpret the raw data
		//     sitting inside a vertex buffer

		// Set up the first element - a position, which is 3 float values
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // How far into the vertex is this?  Assume it's after the previous element
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// Most formats are described as color channels, really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";					// This is "POSITION" - needs to match the semantics in our vertex shader input!
		inputElements[0].SemanticIndex = 0;							// This is the 0th position (there could be more)

		// Set up the second element - a UV, which is 2 more float values
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;			// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";					// Match our vertex shader input!
		inputElements[1].SemanticIndex = 0;							// This is the 0th uv (there could be more)

		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";					// Match our vertex shader input!
		inputElements[2].SemanticIndex = 0;							// This is the 0th normal (there could be more)

		// Set up the fourth element - a tangent, which is 2 more float values
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";					// Match our vertex shader input!
		inputElements[3].SemanticIndex = 0;							// This is the 0th tangent (there could be more)
	
		inputElementsCreated = true;
	}

	//
	// Load in data for Pipeline State
	//
	// IA
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	if (d.contains("topology") && d["topology"].is_string())
	{
		auto input = d["topology"].get<std::string>();
		std::transform(input.begin(), input.end(), input.begin(),
			[](unsigned char c) { return std::toupper(c); });

		if (input == "TRIANGLE") { topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }
		else if (input == "POINT") { topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; }
		else if (input == "LINE") { topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; }
		else if (input == "PATCH") { topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; }
	}

	// RS
	D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
	D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
	bool depthClipEnable = true;
	if (d.contains("rasterizer"))
	{
		if (d["rasterizer"].contains("fill") && d["rasterizer"]["fill"].is_string())
		{
			auto input = d["rasterizer"]["fill"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "SOLID") { fillMode = D3D12_FILL_MODE_SOLID; }
			else if (input == "WIREFRAME") { fillMode = D3D12_FILL_MODE_WIREFRAME; }
		}
		if (d["rasterizer"].contains("cull") && d["rasterizer"]["cull"].is_string())
		{
			auto input = d["rasterizer"]["cull"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "BACK") { cullMode = D3D12_CULL_MODE_BACK; }
			else if (input == "NONE") { cullMode = D3D12_CULL_MODE_NONE; }
			else if (input == "FRONT") { cullMode = D3D12_CULL_MODE_FRONT; }
		}
		if (d["rasterizer"].contains("depthClip") && d["rasterizer"]["depthClip"].is_string())
		{
			auto input = d["rasterizer"]["depthClip"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "FALSE" || input == "0") { depthClipEnable = false; }
		}
	}

	// Depth Stencil
	bool depthEnable = true;
	D3D12_COMPARISON_FUNC compFunc = D3D12_COMPARISON_FUNC_LESS;
	D3D12_DEPTH_WRITE_MASK depthWrite = D3D12_DEPTH_WRITE_MASK_ALL;
	if (d.contains("depthStencil"))
	{
		if (d["depthStencil"].contains("depthEnable") && d["depthStencil"]["depthEnable"].is_string())
		{
			std::string input = d["depthStencil"]["depthEnable"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "FALSE" || input == "0") { depthEnable = false; }
			else { depthEnable = true; }
		}
		if (d["depthStencil"].contains("comparison") && d["depthStencil"]["comparison"].is_string())
		{
			auto input = d["depthStencil"]["comparison"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "NEVER") { compFunc = D3D12_COMPARISON_FUNC_NEVER; }
			else if (input == "LESS") { compFunc = D3D12_COMPARISON_FUNC_LESS; }
			else if (input == "EQUAL") { compFunc = D3D12_COMPARISON_FUNC_EQUAL; }
			else if (input == "LESS_EQUAL" || input == "LESSEQUAL") { compFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; }
			else if (input == "GREATER") { compFunc = D3D12_COMPARISON_FUNC_GREATER; }
			else if (input == "NOT_EQUAL" || input == "NOTEQUAL") { compFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL; }
			else if (input == "GREATER_EQUAL" || input == "GREATEREQUAL") { compFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; }
			else if (input == "ALWAYS") { compFunc = D3D12_COMPARISON_FUNC_ALWAYS; }
		}
		if (d["depthStencil"].contains("write") && d["depthStencil"]["write"].is_string())
		{
			std::string input = d["depthStencil"]["write"].get<std::string>();
			std::transform(input.begin(), input.end(), input.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (input == "ZERO" || input == "FALSE" || input == "0") { depthWrite = D3D12_DEPTH_WRITE_MASK_ZERO; }
			else { depthWrite = D3D12_DEPTH_WRITE_MASK_ALL; }
		}
	}

	// Blend State
	D3D12_BLEND srcBlend = D3D12_BLEND_ONE;
	D3D12_BLEND destBlend = D3D12_BLEND_ZERO;
	D3D12_BLEND_OP blendOp = D3D12_BLEND_OP_ADD;
	if (d.contains("blendState"))
	{
		if (d["blendState"].contains("src") && d["blendState"]["src"].is_number_integer())
		{ srcBlend = d["blendState"]["src"].get<D3D12_BLEND>(); }
		if (d["blendState"].contains("dest") && d["blendState"]["dest"].is_number_integer())
		{ destBlend = d["blendState"]["dest"].get<D3D12_BLEND>(); }
		if (d["blendState"].contains("blendOp") && d["blendState"]["blendOp"].is_number_integer())
		{ blendOp = d["blendState"]["blendOp"].get<D3D12_BLEND_OP>(); }
	}

	// Pipeline state
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = {};
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = topology;
		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();
		// -- Shaders (VS/PS) ---
		psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();
		// -- Render targets ---
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		// -- States ---
		psoDesc.RasterizerState.FillMode = fillMode;
		psoDesc.RasterizerState.CullMode = cullMode;
		psoDesc.RasterizerState.DepthClipEnable = depthClipEnable;
		psoDesc.DepthStencilState.DepthEnable = depthEnable;
		psoDesc.DepthStencilState.DepthFunc = compFunc;
		psoDesc.DepthStencilState.DepthWriteMask = depthWrite;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = srcBlend;
		psoDesc.BlendState.RenderTarget[0].DestBlend = destBlend;
		psoDesc.BlendState.RenderTarget[0].BlendOp = blendOp;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;
		// Create the pipe state object
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc,
			IID_PPV_ARGS(pipelineState.GetAddressOf()));
	}

	pipelineStates.insert({ filename, pipelineState });

	return pipelineState;
}
void Assets::LoadUnknownShader(std::wstring path)
{
	// Load the file into a blob
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
	if (D3DReadFileToBlob(path.c_str(), shaderBlob.GetAddressOf()) != S_OK)
		return;
	// Set up shader reflection to get information about
	// this shader and its variables, buffers, etc.
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl;
	D3DReflect(
		shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)refl.GetAddressOf());
	// Get the description of the shader
	D3D12_SHADER_DESC shaderDesc;
	refl->GetDesc(&shaderDesc);
	// What kind of shader?
	switch (D3D12_SHVER_GET_TYPE(shaderDesc.Version))
	{
	case D3D12_SHVER_PIXEL_SHADER: LoadPixelShader(path); break; // Load file as Pixel shader
	case D3D12_SHVER_VERTEX_SHADER: LoadVertexShader(path); break; // Load file as Vertex shader
	}
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::LoadPixelShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootShaderPath.size();
	size_t shaderPathPosition = path.rfind(rootShaderPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading pixel shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	Microsoft::WRL::ComPtr<ID3DBlob> ps;
	D3DReadFileToBlob(path.c_str(), ps.GetAddressOf());

	// Success
	pixelShaders.insert({ filename, ps });
	return ps;
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::LoadVertexShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootShaderPath.size();
	size_t shaderPathPosition = path.rfind(rootShaderPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading vertex shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	D3DReadFileToBlob(path.c_str(), vs.GetAddressOf());

	// Success
	vertexShaders.insert({ filename, vs });
	return vs;
}

std::shared_ptr<Material> Assets::LoadMaterial(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading material: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Verify required members (pipeline for now)
	if (d.is_discarded() ||
		!d.contains("pipeline") ||
		!d.contains("rootSig"))
	{
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = GetPiplineState(L"PipelineStates/BasicPipelineState");
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig = GetRootSig(L"RootSigs/BasicRootSig");
		std::shared_ptr<Material> matInvalid = std::make_shared<Material>(pipelineState, rootSig);
		AddMaterial(filename, matInvalid);
		return matInvalid;
	}

	// Check to see the requested pipeline state
	std::wstring psName = NarrowToWide(d["pipeline"].get<std::string>());
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = GetPiplineState(psName);

	// Check to see the requested pipeline state
	std::wstring rsName = NarrowToWide(d["rootSig"].get<std::string>());
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig = GetRootSig(rsName);

	// We have enough to make the material
	std::shared_ptr<Material> mat = std::make_shared<Material>(pipelineState, rootSig);

	// Check for 3-component tint
	if (d.contains("tint") && d["tint"].size() == 4)
	{
		DirectX::XMFLOAT4 tint(1, 1, 1, 1);
		tint.x = d["tint"][0].get<float>();
		tint.y = d["tint"][1].get<float>();
		tint.z = d["tint"][2].get<float>();
		tint.w = d["tint"][3].get<float>();
		mat->SetColorTint(tint);
	}

	// 2-component uvScale
	if (d.contains("uvScale") && d["uvScale"].size() == 2)
	{
		DirectX::XMFLOAT2 uvScale(0, 0);
		uvScale.x = d["uvScale"][0].get<float>();
		uvScale.y = d["uvScale"][1].get<float>();
		mat->SetUVScale(uvScale);
	}

	// 2-component uvOffset
	if (d.contains("uvOffset") && d["uvOffset"].size() == 2)
	{
		DirectX::XMFLOAT2 uvOffset(0, 0);
		uvOffset.x = d["uvOffset"][0].get<float>();
		uvOffset.y = d["uvOffset"][1].get<float>();
		mat->SetUVOffset(uvOffset);
	}

	// Roughness
	if (d.contains("roughness"))
	{
		mat->SetRoughness(d["roughness"].get<float>());
	}

	// Check for Topology
	if (d.contains("topology") && d["topology"].is_string())
	{
		D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		auto input = d["topology"].get<std::string>();
		std::transform(input.begin(), input.end(), input.begin(),
			[](unsigned char c) { return std::toupper(c); });

		if (input == "TRIANGLELIST") { topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; }
		else if (input == "TRIANGLESTRIP") { topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; }
		else if (input == "TRIANGLEFAN") { topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN; }
		else if (input == "LINELIST") { topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; }
		else if (input == "LINESTRIP") { topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; }
		else if (input == "POINTLIST") { topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; }

		mat->SetTopology(topology);
	}


	// Check for textures
	if (d.contains("textures"))
	{
		for (unsigned int t = 0; t < d["textures"].size(); t++)
		{
			// Do we know about this texture?
			std::wstring textureName = NarrowToWide(d["textures"][t]["name"].get<std::string>());
			D3D12_CPU_DESCRIPTOR_HANDLE texture = GetTexture(textureName);

			std::string textureType = d["textures"][t]["type"].get<std::string>();
			std::transform(textureType.begin(), textureType.end(), textureType.begin(),
				[](unsigned char c) { return std::toupper(c); });

			if (textureType == "ALBEDO")
			{ mat->AddTexture(texture, 0); }
			else if (textureType == "NORMALS" || textureType == "NORMAL")
			{ mat->AddTexture(texture, 1); }
			else if (textureType == "ROUGHNESS" || textureType == "ROUGH" || textureType == "SPECULAR")
			{ mat->AddTexture(texture, 2); }
			else if (textureType == "METAL" || textureType == "METALNESS")
			{ mat->AddTexture(texture, 3); }
		}
	}

	// Add the material to our list and return it
	mat->FinalizeMaterial();
	materials.insert({ filename, mat });
	return mat;
}

std::shared_ptr<Sky> Assets::LoadSky(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading sky: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json d = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Verify required members (texture)
	if (d.is_discarded() ||
		!d.contains("texture"))
	{
		return 0;
	}

	// Do we know about this texture?
	std::wstring textureName = NarrowToWide(d["texture"].get<std::string>());
	D3D12_CPU_DESCRIPTOR_HANDLE texture = GetTexture(textureName, false, true);

	// We have enough to make the sky
	//Sky asd = Sky(texture);
	std::shared_ptr<Sky> sky = std::make_shared<Sky>(texture);

	// Check for tint
	DirectX::XMFLOAT4 tint(1, 1, 1, 1);
	if (d.contains("tint") && (d["tint"].size() == 4 || d["tint"].size() == 3))
	{
		tint.x = d["tint"][0].get<float>();
		tint.y = d["tint"][1].get<float>();
		tint.z = d["tint"][2].get<float>();
		tint.w = 1.f;
	}
	sky->SetColorTint(tint);

	// Check for lights
	if (d.contains("lights"))
	{
		for (unsigned int t = 0; t < d["lights"].size(); t++)
		{
			Light light = {};
			light.Type = LIGHT_TYPE_DIRECTIONAL;

			// Check for color
			DirectX::XMFLOAT3 color(1, 1, 1);
			if (d["lights"][t].contains("color") && d["lights"][t]["color"].size() == 3)
			{
				color.x = d["lights"][t]["color"][0].get<float>() * tint.x;
				color.y = d["lights"][t]["color"][1].get<float>() * tint.y;
				color.z = d["lights"][t]["color"][2].get<float>() * tint.z;
			}
			light.Color = color;

			// Check for direction
			DirectX::XMFLOAT3 dir(0, 1, 0);
			if (d["lights"][t].contains("direction") && d["lights"][t]["direction"].size() == 3)
			{
				dir.x = d["lights"][t]["direction"][0].get<float>();
				dir.y = d["lights"][t]["direction"][1].get<float>();
				dir.z = d["lights"][t]["direction"][2].get<float>();
			}
			light.Direction = dir;

			float intensity = 1.f;
			if (d["lights"][t].contains("intensity")) { intensity = d["lights"][t]["intensity"].get<float>(); }
			light.Intensity = intensity;

			sky->AddLight(light);
		}
	}

	skies.insert({ filename, sky });
	return sky;
}

std::shared_ptr<Scene> Assets::LoadScene(std::wstring path)
{
	path = FixPath(rootAssetPath + path + L".scene");

	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading scene: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Open the file and parse
	std::ifstream file(path);
	json sceneJson = json::parse(file);
	file.close();

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Check for name
	std::string name = "Scene";
	if (sceneJson.contains("name"))
	{
		name = sceneJson["name"].get<std::string>();
	}

	// Create the scene
	std::shared_ptr<Scene> scene = std::make_shared<Scene>(name);


	// Check for cameras
	if (sceneJson.contains("cameras") && sceneJson["cameras"].is_array())
	{
		//scene->AddCamera(Camera::Parse(sceneJson["cameras"][c]));
		for (int c = 0; c < sceneJson["cameras"].size(); c++)
			scene->AddCamera(ParseCamera(sceneJson["cameras"][c]));
	}
	// Create a default camera if none were loaded
	if (scene->GetCameras().size() == 0)
	{
		scene->AddCamera(std::make_shared<Camera>(0.0f, 0.0f, -5.0f, 5.0f, 0.001f, DirectX::XM_PIDIV2, 1.0f));
	}
	scene->SetCurrentCamera(0);

	// Check for lights
	if (sceneJson.contains("lights") && sceneJson["lights"].is_array())
	{
		for (int l = 0; l < sceneJson["lights"].size(); l++)
			scene->AddLight(ParseLight(sceneJson["lights"][l]));
	}

	// Check for sky
	if (sceneJson.contains("sky") && sceneJson["sky"].is_string())
	{
		scene->SetSky(GetSky(NarrowToWide(sceneJson["sky"].get<std::string>())));
	}
	std::vector skyLights = scene->GetSky()->GetLights();
	for (int i = 0; i < skyLights.size(); i++) 
		{ scene->AddLight(skyLights[i]); }

	// Check for entities
	if (sceneJson.contains("entities") && sceneJson["entities"].is_array())
	{
		for (int e = 0; e < sceneJson["entities"].size(); e++)
		{
			scene->AddEntity(ParseEntity(sceneJson["entities"][e]));

			if (sceneJson["entities"][e].contains("transform")
				&& sceneJson["entities"][e]["transform"].contains("parent")
				&& sceneJson["entities"][e]["transform"]["parent"].is_number_integer())
			{
				int index = sceneJson["entities"][e]["transform"]["parent"].get<int>();
				scene->GetEntities()[e]->GetTransform()->SetParent(
					scene->GetEntities()[index]->GetTransform().get());
			}
		}

	}

	scene->InitialSort();

	return scene;
}

std::shared_ptr<Camera> Assets::ParseCamera(nlohmann::json jsonCamera)
{
	// Defaults
	CameraProjectionType projType = CameraProjectionType::Perspective;
	float moveSpeed = 1.0f;
	float lookSpeed = 0.01f;
	float fov = DirectX::XM_PIDIV2;
	float nearClip = 0.01f;
	float farClip = 1000.0f;
	DirectX::XMFLOAT3 pos = { 0, 0, -5 };
	DirectX::XMFLOAT3 rot = { 0, 0, 0 };

	// Check for each type of data
	if (jsonCamera.contains("type") && jsonCamera["type"].get<std::string>() == "orthographic")
		projType = CameraProjectionType::Orthographic;

	if (jsonCamera.contains("moveSpeed")) moveSpeed = jsonCamera["moveSpeed"].get<float>();
	if (jsonCamera.contains("lookSpeed")) lookSpeed = jsonCamera["lookSpeed"].get<float>();
	if (jsonCamera.contains("fov")) fov = jsonCamera["fov"].get<float>();
	if (jsonCamera.contains("near")) nearClip = jsonCamera["near"].get<float>();
	if (jsonCamera.contains("far")) farClip = jsonCamera["far"].get<float>();
	if (jsonCamera.contains("position") && jsonCamera["position"].size() == 3)
	{
		pos.x = jsonCamera["position"][0].get<float>();
		pos.y = jsonCamera["position"][1].get<float>();
		pos.z = jsonCamera["position"][2].get<float>();
	}
	if (jsonCamera.contains("rotation") && jsonCamera["rotation"].size() == 3)
	{
		rot.x = jsonCamera["rotation"][0].get<float>();
		rot.y = jsonCamera["rotation"][1].get<float>();
		rot.z = jsonCamera["rotation"][2].get<float>();
	}

	// Create the camera
	std::shared_ptr<Camera> cam = std::make_shared<Camera>(
		pos, moveSpeed, lookSpeed, fov, 1.0f, nearClip, farClip, projType);
	cam->GetTransform()->SetRotation(rot);

	return cam;
}

Light Assets::ParseLight(nlohmann::json jsonLight)
{
	// Set defaults
	Light light = {};

	// Check data
	if (jsonLight.contains("type"))
	{
		if (jsonLight["type"].get<std::string>() == "directional") light.Type = LIGHT_TYPE_DIRECTIONAL;
		else if (jsonLight["type"].get<std::string>() == "point") light.Type = LIGHT_TYPE_POINT;
		else if (jsonLight["type"].get<std::string>() == "spot") light.Type = LIGHT_TYPE_SPOT;
	}

	if (jsonLight.contains("direction") && jsonLight["direction"].size() == 3)
	{
		light.Direction.x = jsonLight["direction"][0].get<float>();
		light.Direction.y = jsonLight["direction"][1].get<float>();
		light.Direction.z = jsonLight["direction"][2].get<float>();
	}

	if (jsonLight.contains("position") && jsonLight["position"].size() == 3)
	{
		light.Position.x = jsonLight["position"][0].get<float>();
		light.Position.y = jsonLight["position"][1].get<float>();
		light.Position.z = jsonLight["position"][2].get<float>();
	}

	if (jsonLight.contains("color") && jsonLight["color"].size() == 3)
	{
		light.Color.x = jsonLight["color"][0].get<float>();
		light.Color.y = jsonLight["color"][1].get<float>();
		light.Color.z = jsonLight["color"][2].get<float>();
	}

	if (jsonLight.contains("intensity")) light.Intensity = jsonLight["intensity"].get<float>();
	if (jsonLight.contains("range")) light.Range = jsonLight["range"].get<float>();
	if (jsonLight.contains("spotFalloff")) light.SpotFalloff = jsonLight["spotFalloff"].get<float>();

	return light;
}

std::shared_ptr<Entity> Assets::ParseEntity(nlohmann::json jsonEntity)
{
	Assets& assets = Assets::GetInstance();

	std::shared_ptr<Entity> entity = std::make_shared<Entity>(
		assets.GetMesh(NarrowToWide(jsonEntity["mesh"].get<std::string>())),
		assets.GetMaterial(NarrowToWide(jsonEntity["material"].get<std::string>())));

	// Early out if transform is missing
	if (!jsonEntity.contains("transform")) return entity;
	nlohmann::json tr = jsonEntity["transform"];

	// Handle transform
	DirectX::XMFLOAT3 pos = { 0, 0, 0 };
	DirectX::XMFLOAT3 rot = { 0, 0, 0 };
	DirectX::XMFLOAT3 sc = { 1, 1, 1 };

	if (tr.contains("position") && tr["position"].size() == 3)
	{
		pos.x = tr["position"][0].get<float>();
		pos.y = tr["position"][1].get<float>();
		pos.z = tr["position"][2].get<float>();
	}

	if (tr.contains("rotation") && tr["rotation"].size() == 3)
	{
		rot.x = tr["rotation"][0].get<float>();
		rot.y = tr["rotation"][1].get<float>();
		rot.z = tr["rotation"][2].get<float>();
	}

	if (tr.contains("scale") && tr["scale"].size() == 3)
	{
		sc.x = tr["scale"][0].get<float>();
		sc.y = tr["scale"][1].get<float>();
		sc.z = tr["scale"][2].get<float>();
	}

	entity->GetTransform()->SetPosition(pos);
	entity->GetTransform()->SetRotation(rot);
	entity->GetTransform()->SetScale(sc);
	return entity;
}



// Helpers for determining the actual path to the executable
// Written By vixorien https://github.dev/vixorien/ggp-advanced-demos
// --------------------------------------------------------------------------
// Gets the actual path to this executable
//
// - As it turns out, the relative path for a program is different when 
//    running through VS and when running the .exe directly, which makes 
//    it a pain to properly load external files (like textures)
//    - Running through VS: Current Dir is the *project folder*
//    - Running from .exe:  Current Dir is the .exe's folder
// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
//    Visual Studio "thing", and isn't obvious unless you know to look 
//    for it.  In fact, it could be fixed by changing a setting in VS, but
//    the option is stored in a user file (.suo), which is ignored by most
//    version control packages by default.  Meaning: the option must be
//    changed every on every PC.  Ugh.  So instead, here's a helper.
// --------------------------------------------------------------------------
std::string Assets::GetExePath()
{
	// Assume the path is just the "current directory" for now
	std::string path = ".\\";

	// Get the real, full path to this executable
	char currentDir[1024] = {};
	GetModuleFileNameA(0, currentDir, 1024);

	// Find the location of the last slash charaacter
	char* lastSlash = strrchr(currentDir, '\\');
	if (lastSlash)
	{
		// End the string at the last slash character, essentially
		// chopping off the exe's file name.  Remember, c-strings
		// are null-terminated, so putting a "zero" character in 
		// there simply denotes the end of the string.
		*lastSlash = 0;

		// Set the remainder as the path
		path = currentDir;
	}

	// Toss back whatever we've found
	return path;
}
// ---------------------------------------------------
//  Same as GetExePath(), except it returns a wide character
//  string, which most of the Windows API requires.
// ---------------------------------------------------
std::wstring Assets::GetExePath_Wide()
{
	// Grab the path as a standard string
	std::string path = GetExePath();

	// Convert to a wide string
	wchar_t widePath[1024] = {};
	mbstowcs_s(0, widePath, path.c_str(), 1024);

	// Create a wstring for it and return
	return std::wstring(widePath);
}
// ----------------------------------------------------
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::string Assets::GetFullPathTo(std::string relativeFilePath)
{
	return GetExePath() + "\\" + relativeFilePath;
}
// ----------------------------------------------------
//  Same as GetFullPathTo, but with wide char strings.
// 
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::wstring Assets::GetFullPathTo_Wide(std::wstring relativeFilePath)
{
	return GetExePath_Wide() + L"\\" + relativeFilePath;
}
// ----------------------------------------------------
// Determines if the given string ends with the given ending
// ----------------------------------------------------
bool Assets::EndsWith(std::string str, std::string ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}
bool Assets::EndsWith(std::wstring str, std::wstring ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}
// ----------------------------------------------------
//  Helper function for converting a wide character 
//  string to a standard ("narrow") character string
// ----------------------------------------------------
std::string Assets::ToNarrowString(const std::wstring& str)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), 0, 0, 0, 0);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], size, 0, 0);
	return result;
}
// ----------------------------------------------------
// Remove the file extension by searching for the last 
// period character and removing everything afterwards
// ----------------------------------------------------
std::wstring Assets::RemoveFileExtension(std::wstring str)
{
	size_t found = str.find_last_of('.');
	return str.substr(0, found);
}
