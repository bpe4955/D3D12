#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> _mesh):
    mesh(_mesh)
{
    transform = std::make_shared<Transform>();
}

std::shared_ptr<Transform> Entity::GetTransform() { return transform; }
std::shared_ptr<Mesh> Entity::GetMesh() { return mesh; }

void Entity::setTransform(std::shared_ptr<Transform> _transform) { transform = _transform; }
void Entity::SetMesh(std::shared_ptr<Mesh> _mesh) { mesh = _mesh; }
