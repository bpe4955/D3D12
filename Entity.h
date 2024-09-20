#pragma once
#include <memory>
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> _mesh,
		std::shared_ptr<Material> _material);
	Entity(std::vector<std::shared_ptr<Mesh>> _meshes,
		std::vector<std::shared_ptr<Material>> _materials);

	// Getters
	std::shared_ptr<Transform> GetTransform();
	std::vector<std::shared_ptr<Mesh>> GetMeshes();
	std::vector<std::shared_ptr<Material>> GetMaterials();

	// Setters
	void setTransform(std::shared_ptr<Transform> _transform);
	void SetMeshes(std::vector<std::shared_ptr<Mesh>> _mesh);
	void SetMaterials(std::vector<std::shared_ptr<Material>> _material);

private:
	std::shared_ptr<Transform> transform;
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
};

