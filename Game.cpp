#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "Assets.h"
#include "psapi.h"

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
#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)

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
	for(int i=0; i<Graphics::numCommandLists; i++)
		Graphics::commandList[i]->Close();
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

		/*
		if (scene->GetEmitters().size() == 0)
		{
			scene->GetEmitters().push_back(std::make_shared<Emitter>((int)500, (int)10, (float)15, 
				Assets::GetInstance().GetTexture(L"Textures/Particles/PNG (Black background)/smoke_01")));
			scene->GetEmitters()[0]->GetTransform()->SetPosition(XMFLOAT3(-15, 1, -10));
			scene->GetEmitters()[0]->startVelocity = XMFLOAT3(1, 0, 1);
			scene->GetEmitters()[0]->velocityRandomRange = XMFLOAT3(1, 0, 1); 
			scene->GetEmitters()[0]->acceleration = XMFLOAT3(0, 1, 0);
			scene->GetEmitters()[0]->sizeStartMinMax = XMFLOAT2(0.05f, 0.25f);
			scene->GetEmitters()[0]->sizeEndMinMax = XMFLOAT2(1.0f, 3.0f);
			scene->GetEmitters()[0]->rotationStartMinMax = XMFLOAT2(-3.15f, 3.15f);
			scene->GetEmitters()[0]->rotationEndMinMax = XMFLOAT2(-6.3f, 6.3f);
			scene->GetEmitters()[0]->startColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
			scene->GetEmitters()[0]->endColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);

			scene->GetEmitters().push_back(std::make_shared<Emitter>((int)500, (int)150, (float)2.5,
				Assets::GetInstance().GetTexture(L"Textures/Particles/PNG (Transparent)/trace_07"),
				false));
			scene->GetEmitters()[1]->GetTransform()->SetPosition(XMFLOAT3(0, 25, 0));
			scene->GetEmitters()[1]->positionRandomRange = XMFLOAT3(15, 0, 10);
			scene->GetEmitters()[1]->startVelocity = XMFLOAT3(0, -10, 0);
			scene->GetEmitters()[1]->acceleration = XMFLOAT3(0, -10, 0);
			scene->GetEmitters()[1]->sizeStartMinMax = XMFLOAT2(0.25f, 0.25f);
			scene->GetEmitters()[1]->sizeEndMinMax = XMFLOAT2(0.25f, 0.25f);
			scene->GetEmitters()[1]->startColor = XMFLOAT4(0.77f, 0.85f, 0.1f, 0.6f);
			scene->GetEmitters()[1]->endColor = XMFLOAT4(0.77f, 0.85f, 0.1f, 0.6f);

		}
		*/
	}
	else if (scene->GetName() == "spheres")
	{
		// Entities
		std::vector<std::shared_ptr<Entity>> entities = scene->GetEntities();
		
		std::shared_ptr<Mesh> mesh = Assets::GetInstance().GetMesh(L"Basic Meshes/sphere");
		std::shared_ptr<Material> material = Assets::GetInstance().GetMaterial(L"Materials/cobblestone");
		
		if (entities.size() <= 1)
		{
			int count = 0;
			XMFLOAT3 pos = XMFLOAT3();
			int range = 120;
			for (int z = -range; z < range; z += 16)
			{
				pos.z = (float)z;
				for (int y = -range; y < range; y += 16)
				{
					pos.y = (float)y;
					for (int x = -range; x < range; x += 16)
					{
						count++;
						pos.x = (float)x;
						std::string name = "Sphere" + std::to_string(count);
						std::shared_ptr<Entity> entity = std::make_shared<Entity>(mesh, material, name);
						entity->GetTransform()->SetPosition(pos);

						scene->AddEntity(entity);
					}
				}
			}
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

	// Scene
	scene->Update(deltaTime, totalTime);
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

// Partcile UI data
static float lifeTime[4] = { 5.0f, 5.0f, 5.0f, 5.0f, };
static float pos[4][3] = { 0.0f, 0.0f, 0.0f };
static float posRand[4][3] = { 0.0f, 0.0f, 0.0f };
static float startVel[4][3] = { 0.0f, 0.0f, 0.0f};
static float velRand[4][3] = { 0.0f, 0.0f, 0.0f };
static float acc[4][3] = { 0.0f, 0.0f, 0.0f };
static int particlesPerSecond[4] = { 10, 10, 10, 10 };
//static int maxParticles[4] = { 500, 500, 500, 500 };
static float startRot[4][2] = { 0.0f, 0.0f };
static float endRot[4][2] = { 0.0f, 0.0f };
static float startSize[4][2] = { 1.0f, 1.0f };
static float endSize[4][2] = { 1.0f, 1.0f };
static float startColor[4][4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float endColor[4][4] = { 1.0f, 1.0f, 1.0f, 1.0f };

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
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		//ImGui::Text("Pagefile Usage: %i MB", (int)pmc.PagefileUsage / 125000););
		ImGui::Text("Virtual Memory: %i MB", (int)pmc.PrivateUsage / 125000);
		ImGui::Text("Physical Memory: %i MB", (int)pmc.WorkingSetSize / 125000);


		ImGui::Text("Frame Count: %d", ImGui::GetFrameCount());
		ImGui::Text("Window Resolution: %dx%d", Window::Width(), Window::Height());
		ImGui::Checkbox("ImGui Demo Window Visibility", &showDemoWindow);
		if (ImGui::Button(isFullscreen ? "Windowed" : "Fullscreen")) {
			isFullscreen = !isFullscreen;
			Graphics::swapChain->SetFullscreenState(isFullscreen, NULL);
		}

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Camera"))
	{
		XMFLOAT3 pos = scene->GetCurrentCamera()->GetTransform()->GetPosition();
		ImGui::Text("Position: %f, %f, %f", pos.x, pos.y, pos.z);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Scene"))
	{
		const char* items[] = { "Basic", "Spheres", "Peaches Castle", "Click Clock Wood"};
		static int item_current = 0;
		if (ImGui::Combo("combo", &item_current, items, IM_ARRAYSIZE(items)))
		{
			Graphics::ResizeBuffers(Window::Width(), Window::Height());
			scene->Clear();
			switch (item_current)
			{
			case 0: scene = Assets::GetInstance().LoadScene(L"Scenes/basicScene"); break;
			case 1: scene = Assets::GetInstance().LoadScene(L"Scenes/spheres"); break;
			case 2: scene = Assets::GetInstance().LoadScene(L"Scenes/peachesCastle"); break;
			case 3: scene = Assets::GetInstance().LoadScene(L"Scenes/clickClockWood"); break;
			}
			currentCameraIndex = 0;
			scene->GetCurrentCamera()->UpdateProjectionMatrix(Window::AspectRatio());
			CreateLights();
		}

		ImGui::TreePop();
	}
	if (scene->GetName() == "basicScene")
	if (ImGui::TreeNode("Particle"))
	{
		for (int i = 0; i < scene->GetEmitters().size() && i < 4; i++)
		{
			if (ImGui::TreeNode(std::to_string(i).c_str()))
			{
				if (ImGui::DragInt("ParticlesPerSec", &particlesPerSecond[i], 1, 1, 500))
					scene->GetEmitters()[i]->SetParticlesPerSecond(particlesPerSecond[i]);
				if (ImGui::DragFloat("LifeTime", &lifeTime[i], 1, 1, 50))
					scene->GetEmitters()[i]->lifeTime = lifeTime[i];
				/*if (ImGui::DragInt("Max Particles", &maxParticles[i], 1, 1, 1000))
					scene->GetEmitters()[i]->SetMaxParticles(maxParticles[i]);*/
				if (ImGui::DragFloat3("Position", pos[i], 0.05f, -100.0f, 100.0f))
					scene->GetEmitters()[i]->GetTransform()->SetPosition(XMFLOAT3(pos[i][0], pos[i][1], pos[i][2]));
				if (ImGui::DragFloat3("PositionRandomization", posRand[i], 0.05f, -100.0f, 100.0f))
					scene->GetEmitters()[i]->positionRandomRange = XMFLOAT3(posRand[i][0], posRand[i][1], posRand[i][2]);
				if (ImGui::DragFloat3("StartVelocity", startVel[i], 0.05f, -100.0f, 100.0f))
					scene->GetEmitters()[i]->startVelocity = XMFLOAT3(startVel[i][0], startVel[i][1], startVel[i][2]);
				if (ImGui::DragFloat3("VelocityRandomization", velRand[i], 0.05f, -100.0f, 100.0f))
					scene->GetEmitters()[i]->velocityRandomRange = XMFLOAT3(velRand[i][0], velRand[i][1], velRand[i][2]);
				if (ImGui::DragFloat3("Acceleration", acc[i], 0.05f, -100.0f, 100.0f))
					scene->GetEmitters()[i]->acceleration = XMFLOAT3(acc[i][0], acc[i][1], acc[i][2]);
				if (ImGui::DragFloat2("StartSize", startSize[i], 0.05f, -10.0f, 10.0f))
					scene->GetEmitters()[i]->sizeStartMinMax = XMFLOAT2(startSize[i][0], startSize[i][1]);
				if (ImGui::DragFloat2("EndSize", endSize[i], 0.05f, -10.0f, 10.0f))
					scene->GetEmitters()[i]->sizeEndMinMax = XMFLOAT2(endSize[i][0], endSize[i][1]);
				if (ImGui::DragFloat2("StartRot", startRot[i], 0.05f, -360.0f, 360.0f))
					scene->GetEmitters()[i]->rotationStartMinMax = XMFLOAT2(startRot[i][0], startRot[i][1]);
				if (ImGui::DragFloat2("EndRot", endRot[i], 0.05f, -360.0f, 360.0f))
					scene->GetEmitters()[i]->rotationEndMinMax = XMFLOAT2(endRot[i][0], endRot[i][1]);
				if (ImGui::ColorPicker4("StartColor", startColor[i]))
					scene->GetEmitters()[i]->startColor = XMFLOAT4(startColor[i][0], startColor[i][1], startColor[i][2], startColor[i][3]);
				if (ImGui::ColorPicker4("EndColor", endColor[i]))
					scene->GetEmitters()[i]->endColor = XMFLOAT4(endColor[i][0], endColor[i][1], endColor[i][2], endColor[i][3]);

				ImGui::TreePop();
			}
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

