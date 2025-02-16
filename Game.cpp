#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "Assets.h"

#include "D3D12Helper.h"

#include <DirectXMath.h>

#include "include/ImGui/imgui.h"
#include "include/ImGui/imgui_impl_win32.h"
#include "include/ImGui/imgui_impl_dx12.h"
#include "include/ImGui/imgui_internal.h"

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
	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Window::Handle());
	ID3D12DescriptorHeap* cbvHeap = d3d12Helper.GetImGuiHeap().Get();
	ImGui_ImplDX12_Init(Graphics::Device.Get(), Graphics::numBackBuffers,
		DXGI_FORMAT_R8G8B8A8_UNORM, cbvHeap,
		cbvHeap->GetCPUDescriptorHandleForHeapStart(),
		cbvHeap->GetGPUDescriptorHandleForHeapStart());

	// Game Init
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

	// Assets
	delete& Assets::GetInstance();

	Graphics::swapChain->SetFullscreenState(false, NULL);

	// ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
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

	// ImGui
	ImGuiUpdate(deltaTime);

	// Camera
	std::shared_ptr<Camera> currentCamera = scene->GetCurrentCamera();
	currentCamera->Update(deltaTime);

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
		entities.back()->GetTransform()->Rotate(XMFLOAT3(0, (float)sin(totalTime) / 58, 0));

		if (scene->GetEmitters().size() == 0)
		{
			scene->GetEmitters().push_back(std::make_shared<Emitter>((int)100, (int)5, (float)5));
		}

	}

	if (Input::KeyPress(VK_TAB))
	{
		XMFLOAT3 camPos = currentCamera->GetTransform()->GetPosition();
		printf("Camera Position: %f, %f, %f \n", camPos.x, camPos.y, camPos.z);
		XMFLOAT3 camRot = currentCamera->GetTransform()->GetPitchYawRoll();
		printf("Camera Rotation: %f, %f, %f \n", camRot.x, camRot.y, camRot.z);
		XMFLOAT3 camFrd = currentCamera->GetTransform()->GetForward();
		printf("Camera Forward: %f, %f, %f \n", camFrd.x, camFrd.y, camFrd.z);
	}

	// Lights
	scene->GetLights()[0].Position = currentCamera->GetTransform()->GetPosition();
	scene->GetLights()[0].Direction = MouseDirection();

	// Emitters
	for (std::shared_ptr<Emitter> emitter : scene->GetEmitters())
	{
		emitter->Update(deltaTime, totalTime);
	}

}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	Graphics::RenderOptimized(scene, (UINT)scene->GetLights().size(),
		deltaTime, totalTime);
}


// ImGui
bool showDemoWindow = false;
bool isFullscreen = false;

void Game::ImGuiUpdate(float deltaTime)
{
	// Feed fresh data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)Window::Width();
	io.DisplaySize.y = (float)Window::Height();
	// Reset the frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	// Determine new input capture
	Input::SetKeyboardCapture(io.WantCaptureKeyboard);
	Input::SetMouseCapture(io.WantCaptureMouse);
	// Show the demo window
	if (showDemoWindow)
	{
		ImGui::ShowDemoWindow(&showDemoWindow);
	}
	
	BuildUI();
}

void Game::BuildUI()
{
	D3D12Helper& d3d12Helper = D3D12Helper::GetInstance();

	char buf[128];
	sprintf_s(buf, "Custom Debug %c###CustomDebug", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3]);
	ImGui::Begin(buf, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("App Details"))
	{
		float fps = ImGui::GetIO().Framerate;
		if (fps > 58) { ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Framerate: %f fps", fps); }
		else if (fps > 30) { ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Framerate: %f fps", fps); }
		else { ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Framerate: %f fps", fps); }
		ImGui::Text("Frame Count: %d", ImGui::GetFrameCount());
		ImGui::Text("Window Resolution: %dx%d", Window::Width(), Window::Height());
		ImGui::Checkbox("ImGui Demo Window Visibility", &showDemoWindow);
		if (ImGui::Button(isFullscreen ? "Windowed" : "Fullscreen")) {
			isFullscreen = !isFullscreen;
			Graphics::swapChain->SetFullscreenState(isFullscreen, NULL);
		}

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Scene"))
	{
		const char* items[] = { "Basic", "Peaches Castle", "Click Clock Wood"};
		static int item_current = 0;
		if (ImGui::Combo("combo", &item_current, items, IM_ARRAYSIZE(items)))
		{
			Graphics::ResizeBuffers(Window::Width(), Window::Height());
			scene->Clear();
			switch (item_current)
			{
			case 0: scene = Assets::GetInstance().LoadScene(L"Scenes/basicScene"); break;
			case 1: scene = Assets::GetInstance().LoadScene(L"Scenes/peachesCastle"); break;
			case 2: scene = Assets::GetInstance().LoadScene(L"Scenes/clickClockWood"); break;
			}
			currentCameraIndex = 0;
			scene->GetCurrentCamera()->UpdateProjectionMatrix(Window::AspectRatio());
		}

		ImGui::TreePop();
	}
	
	
	ImGui::End();
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

