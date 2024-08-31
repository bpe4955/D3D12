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
#include "BufferStructs.h"

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
	CreateBasicGeometry();

	// Set up camera
	{
		cameras.push_back(std::make_shared<Camera>(XMFLOAT3(10, 0.75, -5.0f), 1.0f, 0.01f, 90.0f, Window::AspectRatio()));
		currentCameraIndex = 0;
	}
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
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[0].SemanticName = "POSITION"; // Name must match semantic in shader
		inputElements[0].SemanticIndex = 0; // This is the first POSITION semantic
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT; // R32 G32 = float2
		inputElements[1].SemanticName = "TEXCOORD";
		inputElements[1].SemanticIndex = 0; // This is the first TEXCOORD semantic
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[2].SemanticName = "NORMAL";
		inputElements[2].SemanticIndex = 0; // This is the first NORMAL semantic
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[3].SemanticName = "TANGENT";
		inputElements[3].SemanticIndex = 0; // This is the first TANGENT semantic
	}
	// Root Signature
	{
		// Define a table of CBV's (constant buffer views)
		D3D12_DESCRIPTOR_RANGE cbvTable = {};
		cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvTable.NumDescriptors = 1;
		cbvTable.BaseShaderRegister = 0;
		cbvTable.RegisterSpace = 0;
		cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Define the root parameter
		D3D12_ROOT_PARAMETER rootParam = {};
		rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParam.DescriptorTable.NumDescriptorRanges = 1;
		rootParam.DescriptorTable.pDescriptorRanges = &cbvTable;
		// Describe the overall the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = 1;
		rootSig.pParameters = &rootParam;
		rootSig.NumStaticSamplers = 0;
		rootSig.pStaticSamplers = 0;
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
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
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
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../assets/Basic Meshes/sphere.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../assets/Basic Meshes/cube.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../assets/Basic Meshes/helix.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../assets/Basic Meshes/cylinder.obj").c_str()));
	meshes.push_back(std::make_shared<Mesh>(FixPath(L"../../assets/Basic Meshes/torus.obj").c_str()));

	for (std::shared_ptr<Mesh> meshPtr : meshes)
	{
		entities.push_back(std::make_shared<Entity>(meshPtr));
	}
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

	for (int i = 0; i < entities.size(); i++)
	{
		entities[i]->GetTransform()->SetPosition(
			XMFLOAT3((float)i * 5, (float)sin(i + totalTime), 0));
	}


	cameras[currentCameraIndex]->Update(deltaTime);
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
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
		float color[] = { 0.4f, 0.6f, 0.75f, 1.0f };
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
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap =
			D3D12Helper::GetInstance().GetCBVSRVDescriptorHeap();
		Graphics::commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());
		Graphics::commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// Draw
		for (std::shared_ptr<Entity> entityPtr : entities)
		{
			// Fill out a VertexShaderExternalData struct with the entity’s world matrix and the camera’s matrices.
			VertexShaderData data = {};
			data.world = entityPtr->GetTransform()->GetWorldMatrix();
			data.view = cameras[currentCameraIndex]->GetView();
			data.proj = cameras[currentCameraIndex]->GetProjection();
			// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
			// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
			D3D12_GPU_DESCRIPTOR_HANDLE handle =
				D3D12Helper::GetInstance().FillNextConstantBufferAndGetGPUDescriptorHandle(&data, sizeof(VertexShaderData));
			// Use commandList->SetGraphicsRootDescriptorTable(0, handle) to set the handle from the previous line.
			Graphics::commandList->SetGraphicsRootDescriptorTable(0, handle);
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
		D3D12Helper::GetInstance().CloseExecuteAndResetCommandList();
		// Present the current back buffer
		bool vsyncNecessary = Graphics::VsyncState();
		Graphics::swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);
		// Figure out which buffer is next
		Graphics::currentSwapBuffer++;
		if (Graphics::currentSwapBuffer >= Graphics::numBackBuffers)
			Graphics::currentSwapBuffer = 0;
	}

}



