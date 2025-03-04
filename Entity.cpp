#include "Entity.h"
#include <functional>

// Constructors
Entity::Entity(std::shared_ptr<Mesh> _mesh, 
    std::shared_ptr<Material> _material,
    std::string _name) :
    name(_name)
{
    meshes.push_back(_mesh);
    materials.push_back(_material);
    transform = std::make_shared<Transform>();

    dirtyFuncPtr = [this]() { SetTransformDirty(); };
    transform->SetDirtyFunction(dirtyFuncPtr);

    modelAABB = meshes[0]->GetAABB();
    aabb = modelAABB;
    transformDirty = true;
}

Entity::Entity(std::vector<std::shared_ptr<Mesh>> _meshes,
    std::vector<std::shared_ptr<Material>> _materials,
    std::string _name):
    meshes(_meshes),
    materials(_materials),
    name(_name)
{
    transform = std::make_shared<Transform>();

    dirtyFuncPtr = [this]() { SetTransformDirty(); };
    transform->SetDirtyFunction(dirtyFuncPtr);

    modelAABB = meshes[0]->GetAABB();
    aabb = modelAABB;
    transformDirty = true;
}


// Getters
std::shared_ptr<Transform> Entity::GetTransform() { return transform; }
std::vector<std::shared_ptr<Mesh>> Entity::GetMeshes() { return meshes; }
std::vector<std::shared_ptr<Material>> Entity::GetMaterials() { return materials; }
AABB Entity::GetAABB()
{
    if (transformDirty)
    {
        // Rotation means axis are no longer aligned
        //DirectX::XMFLOAT4X4 worldFloat = transform->GetWorldMatrix();
        //DirectX::XMMATRIX wm = DirectX::XMLoadFloat4x4(&worldFloat);
        DirectX::XMFLOAT3 transFloat = transform->GetPosition();
        DirectX::XMFLOAT3 scaleFloat = transform->GetScale();
        DirectX::XMMATRIX transmat = DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&transFloat));
        DirectX::XMMATRIX scalemat = DirectX::XMMatrixScalingFromVector(XMLoadFloat3(&scaleFloat));
        DirectX::XMMATRIX wm = DirectX::XMMatrixMultiply(scalemat, transmat);

        DirectX::XMVECTOR max = DirectX::XMLoadFloat3(&modelAABB.max);
        DirectX::XMVECTOR min = DirectX::XMLoadFloat3(&modelAABB.min);

        max = DirectX::XMVector3Transform(max, wm);
        min = DirectX::XMVector3Transform(min, wm);

        DirectX::XMStoreFloat3(&aabb.max, max);
        DirectX::XMStoreFloat3(&aabb.min, min);
        transformDirty = false;
    }
    return aabb;
}

// Setters
void Entity::setTransform(std::shared_ptr<Transform> _transform) 
{ 
    transform = _transform;
    std::function<void()> funcPtr = [this]() { SetTransformDirty(); };
    transform->SetDirtyFunction(funcPtr);
    transformDirty = true;
}
void Entity::SetMeshes(std::vector<std::shared_ptr<Mesh>> _meshes) { meshes = _meshes; }
void Entity::SetMaterials(std::vector<std::shared_ptr<Material>> _materials) { materials = _materials; }
void Entity::SetAABB(AABB _aabb) { aabb = _aabb; transformDirty = true; }
void Entity::SetTransformDirty() 
{ 
    hasMoved = true;
    transformDirty = true; 
}