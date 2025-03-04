#include "Scene.h"
#include "Assets.h"

using json = nlohmann::json;

#include "PathHelpers.h"
#include "Window.h"

using namespace DirectX;

// Constructor / Destructor
Scene::Scene(std::string _name, AABB _bounds) 
	: name(_name), bounds(_bounds),
	opaqueEntitiesOrganized(false) {}
Scene::~Scene() {}

// Getters
std::string Scene::GetName() { return name; }
std::vector<std::shared_ptr<Entity>>& Scene::GetEntities() { return entities; }
std::vector<std::shared_ptr<Entity>>& Scene::GetOpaqueEntities() { return opaqueEntities; }
std::vector<Light>& Scene::GetLights() { return lights; }
std::vector<std::shared_ptr<Camera>>& Scene::GetCameras() { return cameras; }
std::shared_ptr<Camera> Scene::GetCurrentCamera() { return currentCamera; }
std::shared_ptr<Sky> Scene::GetSky() { return sky; }
std::vector<std::shared_ptr<Emitter>>& Scene::GetEmitters() { return emitters; }
std::shared_ptr<Octree::Node> Scene::GetOctree() { return octree; }
bool Scene::OpaqueReady() { return opaqueEntitiesOrganized; }

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
void Scene::AddEntity(std::shared_ptr<Entity> entity) 
{
	opaqueEntitiesOrganized = false;
	entities.push_back(entity); 
	if(octree)
		octree->AddToPending(entity);
}
void Scene::AddLight(Light light) { lights.push_back(light); }
void Scene::AddCamera(std::shared_ptr<Camera> camera) { cameras.push_back(camera); }
void Scene::AddEmitter(std::shared_ptr<Emitter> emitter) { emitters.push_back(emitter); }

// Functions
void Scene::Clear()
{
	// Clean up any resources we have
	lights.clear();
	cameras.clear();
	entities.clear();
	octree->Clear();

	currentCamera.reset();
	sky.reset();
}

void Scene::Init()
{
	// Build Octree
	octree = std::make_shared<Octree::Node>(bounds, entities);
	octree->Build();
}


void Scene::Update(float deltaTime, float totalTime)
{
	// Emitters
	for (std::shared_ptr<Emitter> emitter : emitters)
	{
		emitter->Update(deltaTime, totalTime);
	}

	// Octree
	octree->Update();
}

