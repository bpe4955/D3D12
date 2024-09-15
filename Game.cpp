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
	CreateLights();

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

	// Load a scene json file
	scene = Assets::GetInstance().LoadScene(L"Scenes/basicScene");
	currentCameraIndex = 0;
	scene->GetCurrentCamera()->UpdateProjectionMatrix(Window::AspectRatio());
}

void Game::CreateLights()
{
	// Create extra lights
	while (scene->GetLights().size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-5.0f, 5.0f), RandomRange(-5.0f, 5.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(2.0f, 10.0f);
		point.Intensity = RandomRange(0.01f, 0.25f);

		// Add to the list
		scene->AddLight(point);
	}
	
	// Make sure we're exactly MAX_LIGHTS big
	scene->GetLights().resize(MAX_LIGHTS);
}


// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	if (!scene.get()) return;
	if (!scene->GetCameras()[0]) return;

	for (std::shared_ptr<Camera> camPtr : scene->GetCameras())
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

	if(scene->GetName() == "basicScene")
	{
		// Entities
		std::vector<std::shared_ptr<Entity>> entities = scene->GetEntities();

		for (int i = 0; i < 5; i++)
		{
			XMFLOAT3 pos = entities[i]->GetTransform()->GetPosition();
			entities[i]->GetTransform()->SetPosition(
				XMFLOAT3(pos.x, (float)sin(i + totalTime), pos.z));
		}
		entities.back()->GetTransform()->SetPosition(XMFLOAT3((float)sin(totalTime), 1, (float)cos(totalTime)));

		// Camera
		std::shared_ptr<Camera> currentCamera = scene->GetCurrentCamera();
		currentCamera->Update(deltaTime);
		//XMFLOAT3 camPos = currentCamera->GetTransform()->GetPosition();
		//printf("Camera Position: %f, %f, %f \n", camPos.x, camPos.y, camPos.z);

		// Lights
		scene->GetLights()[0].Position = currentCamera->GetTransform()->GetPosition();
		scene->GetLights()[0].Direction = MouseDirection();
	}
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
		std::vector<std::shared_ptr<Entity>> entities = scene->GetEntities();;
		for (std::shared_ptr<Entity> entityPtr : entities)
		{
			// Vertex Data
			{
				// Fill out a VertexShaderExternalData struct with the entity’s world matrix and the camera’s matrices.
				VertexShaderData data = {};
				data.world = entityPtr->GetTransform()->GetWorldMatrix();
				data.worldInvTranspose = entityPtr->GetTransform()->GetWorldInverseTransposeMatrix();
				data.view = scene->GetCurrentCamera()->GetView();
				data.proj = scene->GetCurrentCamera()->GetProjection();
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
				psData.colorTint = mat->GetColorTint();
				psData.uvScale = mat->GetUVScale();
				psData.uvOffset = mat->GetUVOffset();
				psData.cameraPosition = scene->GetCurrentCamera()->GetTransform()->GetPosition();
				psData.lightCount = (int)scene->GetLights().size();
				memcpy(psData.lights, &scene->GetLights()[0], sizeof(Light) * MAX_LIGHTS);

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

		//// Render the Sky
		{
			// Set overall pipeline state
			Graphics::commandList->SetPipelineState(Assets::GetInstance().GetPiplineState(L"PipelineStates/Sky").Get());
			// Root sig (must happen before root descriptor table)
			Graphics::commandList->SetGraphicsRootSignature(Assets::GetInstance().GetRootSig(L"RootSigs/Sky").Get());
			// Vertex Data
			{
				// Fill out a SkyVSData struct with the camera’s matrices.
				SkyVSData data = {};
				data.view = scene->GetCurrentCamera()->GetView();
				data.proj = scene->GetCurrentCamera()->GetProjection();
				// Use FillNextConstantBufferAndGetGPUDescriptorHandle() 
				// to copy the above struct to the GPU and get back the corresponding handle to the constant buffer view.
				D3D12_GPU_DESCRIPTOR_HANDLE handle =
					d3d12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&data), sizeof(SkyVSData));
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
				Graphics::commandList->SetGraphicsRootDescriptorTable(1, cbHandlePS);
			}
			// Set the SRV descriptor handle for this sky's textures
			// Note: This assumes that descriptor table 2 is for textures (as per our root sig)
			Graphics::commandList->SetGraphicsRootDescriptorTable(2, scene->GetSky()->GetTextureGPUHandle());
		
			// Grab the vertex buffer view and index buffer view from this entity’s mesh
			D3D12_INDEX_BUFFER_VIEW indexBuffView = scene->GetSky()->GetMesh()->GetIndexBufferView();
			D3D12_VERTEX_BUFFER_VIEW vertexBuffView = scene->GetSky()->GetMesh()->GetVertexBufferView();
			// Set them using IASetVertexBuffers() and IASetIndexBuffer()
			Graphics::commandList->IASetIndexBuffer(&indexBuffView);
			Graphics::commandList->IASetVertexBuffers(0, 1, &vertexBuffView);
		
			// Call DrawIndexedInstanced() using the index count of this entity’s mesh
			Graphics::commandList->DrawIndexedInstanced(scene->GetSky()->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
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

/// <summary>
/// Gets an approximation of a direction vector to the mouse position.
/// Derived from this code on stackoverflow: https://stackoverflow.com/questions/71731722/correct-way-to-generate-3d-world-ray-from-2d-mouse-coordinates
/// </summary>
/// <returns>An approximated direction vector, derived from the mouse's screen position</returns>
DirectX::XMFLOAT3 Game::MouseDirection()
{
	float xpos = (float)Input::GetMouseX();
	float ypos = (float)Input::GetMouseY();

	// converts a position from the 2d xpos, ypos to a normalized 3d direction
	float x = (2.0f * xpos) / Window::Width() - 1.0f;
	float y = 1.0f - (2.0f * ypos) / Window::Height();
	float z = 1.0f;
	XMFLOAT3 ray_nds = XMFLOAT3(x, y, z);
	XMFLOAT4 ray_clip = XMFLOAT4(ray_nds.x, ray_nds.y, ray_nds.z, 1.0f);

	// eye space to clip we would multiply by projection so
	// clip space to eye space is the inverse projection
	XMFLOAT4X4 proj = scene->GetCurrentCamera()->GetProjection();
	XMMATRIX invProj = DirectX::XMMatrixInverse(nullptr, XMLoadFloat4x4(&proj));
	XMVECTOR rayEyeVec = XMVector4Transform(XMLoadFloat4(&ray_clip), invProj);

	// world space to eye space is usually multiply by view so
	// eye space to world space is inverse view
	XMFLOAT4X4 view = scene->GetCurrentCamera()->GetView();
	XMMATRIX viewMatInv = DirectX::XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));

	// Convert float4 to float3 and normalize
	XMFLOAT4 inv_ray_wor;
	XMStoreFloat4(&inv_ray_wor, XMVector4Transform(rayEyeVec, viewMatInv));
	XMFLOAT3 ray_wor = XMFLOAT3(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
	XMVECTOR ray_wor_vec = XMVector3Normalize(XMLoadFloat3(&ray_wor));
	XMStoreFloat3(&ray_wor, ray_wor_vec);
	return ray_wor;
}

