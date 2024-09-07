#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"

#include "D3D12Helper.h"

#include <DirectXMath.h>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Called once per program, after the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	// - You'll be expanding and/or replacing these later
	CreateRootSigAndPipelineState();
	CreateBasicMaterials();
	CreateBasicGeometry();
	CreateBasicEntities();
	CreateLights();

	// Set up camera
	{
		cameras.push_back(std::make_shared<Camera>(XMFLOAT3(10, 0.75, -5.0f), 1.0f, 0.01f, 90.0f, Window::AspectRatio()));
		currentCameraIndex = 0;
	}

	// Ensure the command list is closed going into Draw for the first time
	Graphics::commandList->Close();
}


// --------------------------------------------------------
// Clean up memory or objects created by this class
// 
// Note: Using smart pointers means there probably won't
//       be much to manually clean up here!
// --------------------------------------------------------
Game::~Game()
{
	D3D12Helper::GetInstance().WaitForGPU();
}

// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"PixelShader.cso").c_str(), pixelShaderByteCode.GetAddressOf());
	}
	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {}; 
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
	}

	// Root Signature
	{
		// Describe the range of CBVs needed for the vertex shader
		D3D12_DESCRIPTOR_RANGE cbvRangeVS = {};
		cbvRangeVS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeVS.NumDescriptors = 1;
		cbvRangeVS.BaseShaderRegister = 0;
		cbvRangeVS.RegisterSpace = 0;
		cbvRangeVS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Describe the range of CBVs needed for the pixel shader
		D3D12_DESCRIPTOR_RANGE cbvRangePS = {};
		cbvRangePS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangePS.NumDescriptors = 1;
		cbvRangePS.BaseShaderRegister = 0;
		cbvRangePS.RegisterSpace = 0;
		cbvRangePS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Create a range of SRV's for textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4; // Set to max number of textures at once (match pixel shader!)
		srvRange.BaseShaderRegister = 0; // Starts at s0 (match pixel shader!)
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[3] = {};
		// CBV table param for vertex shader
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVS;
		// CBV table param for pixel shader
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangePS;
		// SRV table param
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;
		// Create a single static sampler (available to all pixel shaders at the same slot)
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0; // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };
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
	}
	// Pipeline state
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState.DepthClipEnable = true;
		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;
		// Create the pipe state object
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc,
			IID_PPV_ARGS(pipelineState.GetAddressOf()));
	}
}

// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Basic Meshes/sphere.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Basic Meshes/cube.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Basic Meshes/helix.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Basic Meshes/cylinder.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Basic Meshes/torus.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../Assets/Models/Pikachu(Gigantamax).fbx").c_str()));
	
}

void Game::CreateBasicMaterials()
{
	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();

	// Load textures
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneAlbedo = 
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/cobblestone_albedo.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneNormals =
		d3d12Helper.LoadTexture(FixPath(L"/../../Assets/Textures/PBR/cobblestone_normals.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneRoughness =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/cobblestone_roughness.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneMetal =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/cobblestone_metal.png").c_str());

	D3D12_CPU_DESCRIPTOR_HANDLE bronzeAlbedo =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/bronze_albedo.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeNormals =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/bronze_normals.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeRoughness =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/bronze_roughness.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeMetal =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/bronze_metal.png").c_str());

	D3D12_CPU_DESCRIPTOR_HANDLE scratchedAlbedo =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/scratched_albedo.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedNormals =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/scratched_normals.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedRoughness =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/scratched_roughness.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedMetal =
		d3d12Helper.LoadTexture(FixPath(L"../../Assets/Textures/PBR/scratched_metal.png").c_str());

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> cobbleMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	cobbleMat->AddTexture(cobblestoneAlbedo, 0);
	cobbleMat->AddTexture(cobblestoneNormals, 1);
	cobbleMat->AddTexture(cobblestoneRoughness, 2);
	cobbleMat->AddTexture(cobblestoneMetal, 3);
	cobbleMat->FinalizeMaterial();
	materials.push_back(cobbleMat);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	bronzeMat->AddTexture(bronzeAlbedo, 0);
	bronzeMat->AddTexture(bronzeNormals, 1);
	bronzeMat->AddTexture(bronzeRoughness, 2);
	bronzeMat->AddTexture(bronzeMetal, 3);
	bronzeMat->FinalizeMaterial();
	materials.push_back(bronzeMat);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	scratchedMat->AddTexture(scratchedAlbedo, 0);
	scratchedMat->AddTexture(scratchedNormals, 1);
	scratchedMat->AddTexture(scratchedRoughness, 2);
	scratchedMat->AddTexture(scratchedMetal, 3);
	scratchedMat->FinalizeMaterial();
	materials.push_back(scratchedMat);
}

void Game::CreateBasicEntities()
{
	for (int i=0; i<meshes.size(); i++)
	{
		entities.push_back(std::make_shared<Entity>(meshes[i], materials[i % materials.size()]));
	}
	entities.back()->GetTransform()->SetScale(0.025f);
	entities.back()->GetTransform()->SetRotation(XMFLOAT3(XM_PIDIV2, 0, 0));



	entities.push_back(std::make_shared<Entity>(meshes[0], materials[0]));
	entities.back()->GetTransform()->SetScale(0.5f);
	entities.back()->GetTransform()->SetParent(entities[0]->GetTransform().get(), true);
}

void Game::CreateLights()
{
	// Reset
	lights.clear();

	// Setup lights
	Light change = {};
	change.Type = LIGHT_TYPE_SPOT;
	change.Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	change.Intensity = 0.5f;
	change.Range = 5.f;
	change.SpotFalloff = 25.f;

	Light dir = {};
	dir.Type = LIGHT_TYPE_DIRECTIONAL;
	dir.Direction = XMFLOAT3(0, 1, -1);
	dir.Color = XMFLOAT3(1.f, 0.f, 0.f);
	dir.Intensity = 1.0f;

	Light poi = {};
	poi.Type = LIGHT_TYPE_POINT;
	poi.Color = XMFLOAT3(0.0f, 0.0f, 1.0f);
	poi.Intensity = 1.0f;
	poi.Position = XMFLOAT3(10.0f, 0, 0);
	poi.Range = 20.f;
	
	Light spot = {};
	spot.Type = LIGHT_TYPE_SPOT;
	spot.Direction = XMFLOAT3(0, 0, 1);
	spot.Color = XMFLOAT3(0.f, 1.f, 0.f);
	spot.Intensity = 1.0f;
	spot.Position = XMFLOAT3(10, 0, -5.f);
	spot.Range = 20.f;
	spot.SpotFalloff = 500.f;

	// Add light to the list
	lights.push_back(change);
	lights.push_back(dir);
	lights.push_back(poi);
	lights.push_back(spot);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-2.0f, 5.0f), RandomRange(-15.0f, 15.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.01f, 0.12f);
	
		// Add to the list
		lights.push_back(point);
	}

	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
}


// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	for (std::shared_ptr<Camera> camPtr : cameras)
	{
		camPtr->UpdateProjectionMatrix(Window::AspectRatio());
	}
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	for (int i = 0; i < entities.size()-1; i++)
	{
		entities[i]->GetTransform()->SetPosition(
			XMFLOAT3((float)i * 5, (float)sin(i + totalTime), 0));
	}
	entities.back()->GetTransform()->SetPosition(XMFLOAT3((float)sin(totalTime), 1, (float)cos(totalTime)));

	cameras[currentCameraIndex]->Update(deltaTime);

	lights[0].Position = cameras[currentCameraIndex]->GetTransform()->GetPosition();
	lights[0].Direction = cameras[currentCameraIndex]->GetTransform()->GetForward();
	//XMFLOAT3 camPos = cameras[currentCameraIndex]->GetTransform()->GetPosition();
	//printf("Camera Position: %f, %f, %f \n", camPos.x, camPos.y, camPos.z);
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
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

	// Rendering here!
	{
		// Set overall pipeline state
		Graphics::commandList->SetPipelineState(pipelineState.Get());
		// Root sig (must happen before root descriptor table)
		Graphics::commandList->SetGraphicsRootSignature(rootSignature.Get());
		// Set up other commands for rendering
		Graphics::commandList->OMSetRenderTargets(1, &Graphics::rtvHandles[Graphics::currentSwapBuffer], true, &Graphics::dsvHandle);
		Graphics::commandList->RSSetViewports(1, &Graphics::viewport);
		Graphics::commandList->RSSetScissorRects(1, &Graphics::scissorRect);
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = d3d12Helper.GetCBVSRVDescriptorHeap();
		Graphics::commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());
		Graphics::commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// Draw
		for (std::shared_ptr<Entity> entityPtr : entities)
		{
			// Vertex Data
			{
				// Fill out a VertexShaderExternalData struct with the entity’s world matrix and the camera’s matrices.
				VertexShaderData data = {};
				data.world = entityPtr->GetTransform()->GetWorldMatrix();
				data.worldInvTranspose = entityPtr->GetTransform()->GetWorldInverseTransposeMatrix();
				data.view = cameras[currentCameraIndex]->GetView();
				data.proj = cameras[currentCameraIndex]->GetProjection();
				// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
				// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
				D3D12_GPU_DESCRIPTOR_HANDLE handle =
					d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&data), sizeof(VertexShaderData));
				// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
				Graphics::commandList->SetGraphicsRootDescriptorTable(0, handle);
			}

			// Pixel Shader
			std::shared_ptr<Material> mat = entityPtr->GetMaterial();
			Graphics::commandList->SetPipelineState(mat->GetPipelineState().Get());// Set the SRV descriptor handle for this material's textures
			{
				PixelShaderData psData = {};
				psData.uvScale = mat->GetUVScale();
				psData.uvOffset = mat->GetUVOffset();
				psData.cameraPosition = cameras[currentCameraIndex]->GetTransform()->GetPosition();
				psData.lightCount = (int)lights.size();
				memcpy(psData.lights, &lights[0], sizeof(Light) * MAX_LIGHTS);

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psData), sizeof(PixelShaderData));

				// Set this constant buffer handle
				// Note: This assumes that descriptor table 1 is the
				//       place to put this particular descriptor.  This
				//       is based on how we set up our root signature.
				Graphics::commandList->SetGraphicsRootDescriptorTable(1, cbHandlePS);
			}
			// Set the SRV descriptor handle for this material's textures
			// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
			Graphics::commandList->SetGraphicsRootDescriptorTable(2, mat->GetFinalGPUHandleForTextures());


			// Grab the vertex buffer view and index buffer view from this entity’s mesh
			D3D12_INDEX_BUFFER_VIEW indexBuffView = entityPtr->GetMesh()->GetIndexBufferView();
			D3D12_VERTEX_BUFFER_VIEW vertexBuffView = entityPtr->GetMesh()->GetVertexBufferView();
			// Set them using IASetVertexBuffers() and IASetIndexBuffer()
			Graphics::commandList->IASetIndexBuffer(&indexBuffView);
			Graphics::commandList->IASetVertexBuffers(0, 1, &vertexBuffView);

			// Call DrawIndexedInstanced() using the index count of this entity’s mesh
			Graphics::commandList->DrawIndexedInstanced(entityPtr->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
		}

	}

	// Present
	{
		// Transition back to present
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = currentBackBuffer.Get();
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



