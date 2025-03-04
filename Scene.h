#pragma once
#include <string>
#include <memory>
#include "BufferStructs.h"
#include "Camera.h"
#include "Entity.h"
#include "Sky.h"
#include "Emitter.h"
#include "Octree.h"

#include <fstream>
#include "nlohmann/json.hpp"

class Scene
{
public:
	Scene(std::string _name, AABB _bounds);
	~Scene();


	// Getters
	std::string GetName();
	std::vector<std::shared_ptr<Entity>>& GetEntities();
	std::vector<std::shared_ptr<Entity>>& GetOpaqueEntities();
	std::vector<Light>& GetLights();
	std::vector<std::shared_ptr<Camera>>& GetCameras();
	std::shared_ptr<Camera> GetCurrentCamera();
	std::shared_ptr<Sky> GetSky();
	std::vector<std::shared_ptr<Emitter>>& GetEmitters();
	std::shared_ptr<Octree::Node> GetOctree();
	bool OpaqueReady();

	// Setters
	void SetName(std::string _name);
	void SetCurrentCamera(std::shared_ptr<Camera> camera);
	void SetCurrentCamera(unsigned int cameraIndex);
	void SetSky(std::shared_ptr<Sky> _sky);
	
	// Modifiers
	void AddEntity(std::shared_ptr<Entity> entity);
	void AddLight(Light light);
	void AddCamera(std::shared_ptr<Camera> camera);
	void AddEmitter(std::shared_ptr<Emitter> emitter);

	// Functions
	void Clear();
	void Init();
	void Update(float deltaTime, float totalTime);

private:
	std::string name;

	// Vectors of various scene elements
	std::vector<std::shared_ptr<Entity>> entities; // Could also create a map / trie for specific entities
	std::vector<Light> lights;
	std::vector<std::shared_ptr<Camera>> cameras;
	std::vector<std::shared_ptr<Emitter>> emitters;

	// Singular elements
	std::shared_ptr<Camera> currentCamera;
	std::shared_ptr<Sky> sky;

	// For drawing
	AABB bounds;
	std::shared_ptr<Octree::Node> octree;
	bool opaqueEntitiesOrganized;
	std::vector<std::shared_ptr<Entity>> opaqueEntities;
};

