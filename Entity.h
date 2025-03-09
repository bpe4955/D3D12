#pragma once
#include <memory>
#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include <string>

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> _mesh,
		std::shared_ptr<Material> _material,
		std::string _name = "NoName");
	Entity(std::vector<std::shared_ptr<Mesh>> _meshes,
		std::vector<std::shared_ptr<Material>> _materials,
		std::string _name = "NoName");

	// Getters
	std::shared_ptr<Transform> GetTransform();
	std::vector<std::shared_ptr<Mesh>> GetMeshes();
	std::vector<std::shared_ptr<Material>> GetMaterials();
	AABB GetAABB();
	Visibility GetVisibility();

	// Setters
	void SetTransform(std::shared_ptr<Transform> _transform);
	void SetMeshes(std::vector<std::shared_ptr<Mesh>> _mesh);
	void SetMaterials(std::vector<std::shared_ptr<Material>> _material);
	void SetAABB(AABB aabb);
	void SetTransformDirty();
	void SetColorTint(DirectX::XMFLOAT4 _colorTint);

	bool hasMoved = false;

private:
	std::string name;
	std::shared_ptr<Transform> transform;
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	AABB modelAABB;
	AABB aabb;
	bool transformDirty;
	Visibility visibility;
	bool visibilityDirty;

	// Delegates and Callbacks
	std::function<void()> dirtyTransFuncPtr;
	std::function<void()> dirtyVisFuncPtr;
};

