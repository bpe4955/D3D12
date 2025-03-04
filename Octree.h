#pragma once

#define NUM_CHILDREN 8
#define MIN_BOUNDS 0.5

#include <vector>
#include <queue>
#include <stack>
#include "Entity.h"

// https://www.youtube.com/watch?v=L6aYpPAvalI&list=PLysLvOneEETPlOI_PI4mJnocqIpr2cSHS&index=26
// https://github.dev/Cascioli-IGM/106-personal-repo-bpe4955
namespace Octree
{
	enum class Octant : unsigned char {
		O1 = 0x01,	// 0b00000001
		O2 = 0x02,	// 0b00000010
		O3 = 0x04,	// 0b00000100
		O4 = 0x08,	// 0b00001000
		O5 = 0x10,	// 0b00010000
		O6 = 0x20,	// 0b00100000
		O7 = 0x40,	// 0b01000000
		O8 = 0x80,	// 0b10000000
	};

	/// <summary>
	/// A node that can have eight child nodes that divide its 3D space. 
	/// It can hold data or give data to its children to hold.
	/// </summary>
	class Node {
	public:
		// Constructors
		Node();
		Node(AABB _bounds);
		Node(AABB _bounds, 
			std::vector<std::shared_ptr<Entity>> _entities,
			Node* _parent = nullptr);

		// Destructors
		~Node();
		void Clear();

		// Modifiers
		void AddToPending(std::shared_ptr<Entity> _entity);
		void ProcessPending();

		// Getters
		bool HasChildren();
		AABB GetBounds();
		std::vector<std::shared_ptr<Entity>> GetEntities();
		std::vector<std::shared_ptr<Entity>> GetAllEntities();
		Octree::Node** GetChildren();
		unsigned char GetActiveOctants();

		// Utility
		Octree::Node* GetContainingOctant(AABB aabb);
		Octree::Node* GetContainingOctant(std::vector<DirectX::XMFLOAT3> points);

		// Functions
		void Build();
		void Update();
	private:
		// The maximum number of entities in an oct
		// before a subdivision occurs
		//const int MaxEntitiesBeforeSubdivide = 9;

		// The Entities held at this level of the tree
		std::vector<std::shared_ptr<Entity>> entities;

		// This Oct's 3D bounds
		AABB bounds;

		// This oct's parent
		Octree::Node* parent;

		// This oct's children
		Octree::Node* children[NUM_CHILDREN];
		unsigned char activeOctants;

		// Building the tree
		std::queue<std::shared_ptr<Entity>> queue;
		bool treeReady;
		bool treeBuilt;

		// Maintaining the tree
		short MaxLifespan = 8;
		short currentLifespan = -1;

		// Helpers
		AABB CalculateChildBounds(Octant octant);
		bool Insert(std::shared_ptr<Entity> _entity);
	};
}