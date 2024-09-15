#pragma once
#include <string>
#include <memory>
#include "BufferStructs.h"
#include "Camera.h"
#include "Entity.h"
#include "Sky.h"

#include <fstream>
#include "nlohmann/json.hpp"

class Scene
{
public:
	Scene(std::string _name);
	~Scene();


	// Getters
	std::string GetName();
	std::vector<std::shared_ptr<Entity>>& GetEntities();
	std::vector<Light>& GetLights();
	std::vector<std::shared_ptr<Camera>>& GetCameras();
	std::shared_ptr<Camera> GetCurrentCamera();
	std::shared_ptr<Sky> GetSky();

	// Setters
	void SetName(std::string _name);
	void SetCurrentCamera(std::shared_ptr<Camera> camera);
	void SetCurrentCamera(unsigned int cameraIndex);
	void SetSky(std::shared_ptr<Sky> _sky);
	
	// Modifiers
	void AddEntity(std::shared_ptr<Entity> entity);
	void AddLight(Light light);
	void AddCamera(std::shared_ptr<Camera> camera);

	// Functions
	void Clear();

private:
	std::string name;

	// Vectors of various scene elements
	std::vector<std::shared_ptr<Entity>> entities;
	std::vector<Light> lights;
	std::vector<std::shared_ptr<Camera>> cameras;

	// Singular elements
	std::shared_ptr<Camera> currentCamera;
	std::shared_ptr<Sky> sky;

};

