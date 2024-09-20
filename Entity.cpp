#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> _mesh, std::shared_ptr<Material> _material)
{
    meshes.push_back(_mesh);
    materials.push_back(_material);
    transform = std::make_shared<Transform>();
}

Entity::Entity(std::vector<std::shared_ptr<Mesh>> _meshes,
    std::vector<std::shared_ptr<Material>> _materials):
    meshes(_meshes),
    materials(_materials)
{
    transform = std::make_shared<Transform>();
}

std::shared_ptr<Transform> Entity::GetTransform() { return transform; }
std::vector<std::shared_ptr<Mesh>> Entity::GetMeshes() { return meshes; }
std::vector<std::shared_ptr<Material>> Entity::GetMaterials() { return materials; }

void Entity::setTransform(std::shared_ptr<Transform> _transform) { transform = _transform; }
void Entity::SetMeshes(std::vector<std::shared_ptr<Mesh>> _meshes) { meshes = _meshes; }
void Entity::SetMaterials(std::vector<std::shared_ptr<Material>> _materials) { materials = _materials; }
