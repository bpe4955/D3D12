#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "BufferStructs.h"

#include "Camera.h"
#include "Entity.h"
#include "Sky.h"
#include "Scene.h"

class Game
{
public:
	// Basic OOP setup
	Game() = default;
	~Game();
	Game(const Game&) = delete; // Remove copy constructor
	Game& operator=(const Game&) = delete; // Remove copy-assignment operator

	// Primary functions
	void Initialize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void OnResize();

private:
	// Initialization helper methods 
	void LoadAssets();
	void CreateLights();

	std::shared_ptr<Scene> scene;
	unsigned int currentCameraIndex;

	// ImGui
	bool showDemoWindow = true;

	// Helper Functions
	void ImGuiUpdate(float deltaTime);
	void BuildUI();
	DirectX::XMFLOAT3 MouseDirection();
};

