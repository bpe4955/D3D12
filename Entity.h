#pragma once
#include <memory>
#include "Transform.h"
#include "Mesh.h"

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> _mesh);

	// Getters
	std::shared_ptr<Transform> GetTransform();
	std::shared_ptr<Mesh> GetMesh();

	// Setters
	void setTransform(std::shared_ptr<Transform> _transform);
	void SetMesh(std::shared_ptr<Mesh> _mesh);

private:
	std::shared_ptr<Transform> transform;
	std::shared_ptr<Mesh> mesh;
};

