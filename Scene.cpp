#include "Scene.h"
#include "Assets.h"

using json = nlohmann::json;

#include "PathHelpers.h"
#include "Window.h"

using namespace DirectX;

// Constructor / Destructor
Scene::Scene(std::string _name) : name(_name) {}
Scene::~Scene() {}

// Getters
std::string Scene::GetName() { return name; }
std::vector<std::shared_ptr<Entity>>& Scene::GetEntities() { return entities; }
std::vector<Light>& Scene::GetLights() { return lights; }
std::vector<std::shared_ptr<Camera>>& Scene::GetCameras() { return cameras; }
std::shared_ptr<Camera> Scene::GetCurrentCamera() { return currentCamera; }
std::shared_ptr<Sky> Scene::GetSky() { return sky; }

// Setters
void Scene::SetName(std::string _name) { name = _name; }
void Scene::SetCurrentCamera(std::shared_ptr<Camera> camera)
{
	if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end()) 
	{ AddCamera(camera); }
	currentCamera = camera;
}
void Scene::SetCurrentCamera(unsigned int cameraIndex)
{ if (cameraIndex < cameras.size()) { currentCamera = cameras[cameraIndex]; } }
void Scene::SetSky(std::shared_ptr<Sky> _sky) { sky = _sky; }

// Modifiers
void Scene::AddEntity(std::shared_ptr<Entity> entity) { entities.push_back(entity); }
void Scene::AddLight(Light light) { lights.push_back(light); }
void Scene::AddCamera(std::shared_ptr<Camera> camera) { cameras.push_back(camera); }

// Functions
void Scene::Clear()
{
	// Clean up any resources we have
	lights.clear();
	cameras.clear();
	entities.clear();

	currentCamera.reset();
	sky.reset();
}

