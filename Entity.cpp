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
        DirectX::XMFLOAT4X4 worldFloat = transform->GetWorldMatrix();
        DirectX::XMMATRIX wm = DirectX::XMLoadFloat4x4(&worldFloat);
        //DirectX::XMFLOAT3 transFloat = transform->GetPosition();
        //DirectX::XMFLOAT3 scaleFloat = transform->GetScale();
        //DirectX::XMMATRIX transmat = DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&transFloat));
        //DirectX::XMMATRIX scalemat = DirectX::XMMatrixScalingFromVector(XMLoadFloat3(&scaleFloat));
        //DirectX::XMMATRIX wm = DirectX::XMMatrixMultiply(scalemat, transmat);
        //Transform* parent = transform->GetParent();
        //if (parent)
        //{
        //    DirectX::XMFLOAT3 ptransFloat = parent->GetPosition();
        //    DirectX::XMFLOAT3 pscaleFloat = parent->GetScale();
        //    DirectX::XMMATRIX ptransmat = DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&ptransFloat));
        //    DirectX::XMMATRIX pscalemat = DirectX::XMMatrixScalingFromVector(XMLoadFloat3(&pscaleFloat));
        //    DirectX::XMMATRIX pwm = DirectX::XMMatrixMultiply(pscalemat, ptransmat);
        //    wm *= pwm;
        //}

        // Have to transform corners then calculate AABB
        // Could simply transform max and min, but rotation makes things messy
        DirectX::XMFLOAT3 corners[8] = {
                {modelAABB.min.x, modelAABB.min.y, modelAABB.min.z}, // x y z
                {modelAABB.max.x, modelAABB.min.y, modelAABB.min.z}, // X y z
                {modelAABB.min.x, modelAABB.max.y, modelAABB.min.z}, // x Y z
                {modelAABB.max.x, modelAABB.max.y, modelAABB.min.z}, // X Y z

                {modelAABB.min.x, modelAABB.min.y, modelAABB.max.z}, // x y Z
                {modelAABB.max.x, modelAABB.min.y, modelAABB.max.z}, // X y Z
                {modelAABB.min.x, modelAABB.max.y, modelAABB.max.z}, // x Y Z
                {modelAABB.max.x, modelAABB.max.y, modelAABB.max.z}, // X Y Z
            };
        // Transform corners
        for (int c = 0; c < 8; c++)
        {
            DirectX::XMStoreFloat3(&corners[c],
                DirectX::XMVector3Transform(
                    DirectX::XMLoadFloat3(&corners[c])
                    , wm));
        }

        // Calculate AABB
        aabb.max = DirectX::XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        aabb.min = DirectX::XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        for (int c = 0; c < 8; c++)
        {
            aabb.max.x = aabb.max.x > corners[c].x ? aabb.max.x : corners[c].x;
            aabb.max.y = aabb.max.y > corners[c].y ? aabb.max.y : corners[c].y;
            aabb.max.z = aabb.max.z > corners[c].z ? aabb.max.z : corners[c].z;
            aabb.min.x = aabb.min.x < corners[c].x ? aabb.min.x : corners[c].x;
            aabb.min.y = aabb.min.y < corners[c].y ? aabb.min.y : corners[c].y;
            aabb.min.z = aabb.min.z < corners[c].z ? aabb.min.z : corners[c].z;
        }

        transformDirty = false;
    }
    return aabb;
}

// Setters
void Entity::setTransform(std::shared_ptr<Transform> _transform) 
{ 
    transform->SetDirtyFunction(nullptr);
    transform = _transform;
    std::function<void()> funcPtr = [this]() { SetTransformDirty(); };
    transform->SetDirtyFunction(funcPtr);
    SetTransformDirty();
}
void Entity::SetMeshes(std::vector<std::shared_ptr<Mesh>> _meshes) { meshes = _meshes; }
void Entity::SetMaterials(std::vector<std::shared_ptr<Material>> _materials) { materials = _materials; }
void Entity::SetAABB(AABB _aabb) { aabb = _aabb; SetTransformDirty(); }
void Entity::SetTransformDirty() 
{ 
    hasMoved = true;
    transformDirty = true; 
}