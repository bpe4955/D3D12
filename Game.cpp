#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "Assets.h"

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
	LoadAssets();
	
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	// - You'll be expanding and/or replacing these later
	CreateBasicMaterials();
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

	delete& Assets::GetInstance();
}

void Game::LoadAssets()
{
	// Initialize the asset manager and set it up to load assets on demand
	// Note: You could call LoadAllAssets() to load literally everything
	//       found in the asstes folder, but "load on demand" is more efficient
	Assets::GetInstance().Initialize(L"../../Assets/", L"./",
		Graphics::Device, true);
}


void Game::CreateBasicMaterials()
{
	Assets& assets = Assets::GetInstance();
	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	materials.push_back(assets.GetMaterial(L"Materials/cobblestone"));
	materials.push_back(assets.GetMaterial(L"Materials/bronze"));
	materials.push_back(assets.GetMaterial(L"Materials/scratched"));
}

void Game::CreateBasicEntities()
{
	Assets& assets = Assets::GetInstance();
	int i = 0;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/sphere"), 
		materials[i % materials.size()])); i++;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/cube"),
		materials[i % materials.size()])); i++;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/helix"),
		materials[i % materials.size()])); i++;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/cylinder"),
		materials[i % materials.size()])); i++;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/torus"),
		materials[i % materials.size()])); i++;
	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Models/Pikachu(Gigantamax)"),
		materials[i % materials.size()])); i++;

	entities.back()->GetTransform()->SetScale(0.025f);
	entities.back()->GetTransform()->SetRotation(XMFLOAT3(XM_PIDIV2, 0, 0));

	entities.push_back(std::make_shared<Entity>(assets.GetMesh(L"Basic Meshes/sphere"),
		materials[0]));
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
		Graphics::commandList->SetPipelineState(Assets::GetInstance().GetPiplineState(L"PipelineStates/BasicCullNone").Get());
		// Root sig (must happen before root descriptor table)
		Graphics::commandList->SetGraphicsRootSignature(Assets::GetInstance().GetRootSig(L"RootSigs/BasicRootSig").Get());
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



