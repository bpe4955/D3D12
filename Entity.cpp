#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> _mesh, std::shared_ptr<Material> _material):
    mesh(_mesh),
    material(_material)
{
    transform = std::make_shared<Transform>();
}

std::shared_ptr<Transform> Entity::GetTransform() { return transform; }
std::shared_ptr<Mesh> Entity::GetMesh() { return mesh; }
std::shared_ptr<Material> Entity::GetMaterial() { return material; }

void Entity::setTransform(std::shared_ptr<Transform> _transform) { transform = _transform; }
void Entity::SetMesh(std::shared_ptr<Mesh> _mesh) { mesh = _mesh; }
void Entity::SetMaterial(std::shared_ptr<Material> _material) { material = _material; }
