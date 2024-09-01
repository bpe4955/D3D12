#pragma once
#include <memory>
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> _mesh, std::shared_ptr<Material> _material);

	// Getters
	std::shared_ptr<Transform> GetTransform();
	std::shared_ptr<Mesh> GetMesh();
	std::shared_ptr<Material> GetMaterial();

	// Setters
	void setTransform(std::shared_ptr<Transform> _transform);
	void SetMesh(std::shared_ptr<Mesh> _mesh);
	void SetMaterial(std::shared_ptr<Material> _material);

private:
	std::shared_ptr<Transform> transform;
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Material> material;
};

