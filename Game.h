#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "BufferStructs.h"

#include "Camera.h"
#include "Entity.h"

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

	// Initialization helper methods - feel free to customize, combine, remove, etc.
	//void LoadShaders();
	void LoadAssets();
	void CreateBasicMaterials();
	void CreateBasicEntities();
	void CreateLights();

	// Note the usage of ComPtr below
	//  - This is a smart pointer for objects that abide by the
	//     Component Object Model, which DirectX objects do
	//  - More info here: https://github.com/Microsoft/DirectXTK/wiki/ComPtr

	// Ideally later move this into a Scene object
	std::vector<std::shared_ptr<Camera>> cameras;
	unsigned int currentCameraIndex;

	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<Entity>> entities;
	std::vector<Light> lights;
};

