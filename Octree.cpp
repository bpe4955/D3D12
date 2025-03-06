#include "Octree.h"


//
//  Constructors
//
Octree::Node::Node():
    parent(nullptr),
    children(),
    activeOctants(0x00),
    treeReady(false),
    treeBuilt(false)
{
    bounds = AABB();
    bounds.min = DirectX::XMFLOAT3();
    bounds.max = DirectX::XMFLOAT3();

    entities = std::vector<std::shared_ptr<Entity>>();
}

Octree::Node::Node(AABB _bounds) :
    parent(nullptr),
    children(),
    activeOctants(0x00),
    treeReady(false),
    treeBuilt(false),
    bounds(_bounds) 
{
    entities = std::vector<std::shared_ptr<Entity>>();
}

Octree::Node::Node(AABB _bounds, 
    std::vector<std::shared_ptr<Entity>> _entities,
    Node* _parent) :
    parent(_parent),
    children(),
    activeOctants(0x00),
    treeReady(false),
    treeBuilt(false),
    bounds(_bounds)
{
    entities = std::vector<std::shared_ptr<Entity>>();
    entities.insert(entities.end(), _entities.begin(), _entities.end());
}


//
//  Destruction
//
Octree::Node::~Node()
{
    Clear();
}

void Octree::Node::Clear()
{
    if (HasChildren())
    {
        for (unsigned char flags = activeOctants, i = 0;
            flags > 0;
            flags >>= 1, i++)
        {
            if (flags & (1 << 0)) // Active Child
                if (children[i] != nullptr)
                {
                    children[i]->Clear();
                    delete children[i];
                    children[i] = nullptr;
                }
        }
    }

    // Clear this object
    entities.clear();
    std::queue<std::shared_ptr<Entity>> empty;
    std::swap(queue, empty);

    treeBuilt = false;
    treeReady = false;

    bounds.min = DirectX::XMFLOAT3();
    bounds.max = DirectX::XMFLOAT3();
}

//
//  Modifiers
//
void Octree::Node::AddToPending(std::shared_ptr<Entity> _entity)
{
    queue.push(_entity);
}
void Octree::Node::ProcessPending()
{
    if (!treeBuilt)
    {
        // Add objects to be sorted during Build
        while (!queue.empty())
        {
            entities.push_back(queue.front());
            queue.pop();
        }
        Build();
    }
    else {
        // Insert the objects
        for (int i = 0, n = (int)queue.size(); i < n; i++)
        {
            if (!Insert(queue.front())) // Didn't fit in the octree
                queue.push(queue.front()); // Put back into queue to check again later
            queue.pop();
        }
    }
}

//
//  Getters
//
bool Octree::Node::HasChildren() { return activeOctants != 0x00; }
AABB Octree::Node::GetBounds() { return bounds; }
std::vector<std::shared_ptr<Entity>> Octree::Node::GetEntities() { return entities; }
std::vector<std::shared_ptr<Entity>> Octree::Node::GetAllEntities()
{
    std::vector<std::shared_ptr<Entity>> final;

    // Get current node entities
    if(entities.size() != 0)
        final.insert(final.end(), entities.begin(), entities.end());

    // Get child node entities
    if (HasChildren())
    {
        for (unsigned char flags = activeOctants, i = 0;
            flags > 0;
            flags >>= 1, i++)
        {
            if (flags & (1 << 0) && children[i] != nullptr) // Child exists
            {
                std::vector<std::shared_ptr<Entity>> child = children[i]->GetAllEntities();
                final.insert(final.end(), child.begin(), child.end());
            }
        }
    }

    return final;
}
std::vector<std::shared_ptr<Entity>> Octree::Node::GetRelevantEntities(Frustum& frustum)
{
    std::vector<std::shared_ptr<Entity>> final;

    bool within = true;
    for (int i = 0; i<6; i++)
    {
        if (!this->bounds.IntersectsPlane(frustum.normals[i]))
        {
            within = false;
            break;
        }
    }
    if (within)
    {
        // Get current node entities
        if (entities.size() != 0)
            final.insert(final.end(), entities.begin(), entities.end());

        // Get child node entities
        if (HasChildren())
        {
            for (unsigned char flags = activeOctants, i = 0;
                flags > 0;
                flags >>= 1, i++)
            {
                if (flags & (1 << 0) && children[i] != nullptr) // Child exists
                {
                    std::vector<std::shared_ptr<Entity>> child = children[i]->GetRelevantEntities(frustum);
                    final.insert(final.end(), child.begin(), child.end());
                }
            }
        }
    }

    return final;
}
Octree::Node** Octree::Node::GetChildren() { return children; }
unsigned char Octree::Node::GetActiveOctants() { return activeOctants; }

//
//  Utility
//
/// <summary>
/// A recursive method that returns the smallest
/// octant that contains the specified axis-aligned bounding box
/// </summary>
/// <param name="aabb">The box to check</param>
/// <returns>The smallest octant that contains the box</returns>
Octree::Node* Octree::Node::GetContainingOctant(AABB aabb)
{
    // Return null if this octant doesn't contain the box
    if (!bounds.Contains(aabb))
        return nullptr;

    // Check Children
    if (HasChildren())
    {
        for (unsigned char flags = activeOctants, i = 0;
            flags > 0;
            flags >>= 1, i++)
        {
            if (flags & (1 << 0) && children[i] != nullptr) // Child exists
            {
                // If box is in child, search within that child
                if (children[i]->bounds.Contains(aabb))
                    return children[i]->GetContainingOctant(aabb);
            }
        }
    }

    // Box only fits in current octant
    return this;
}
/// <summary>
/// A recursive method that returns the smallest
/// octant that contains the specified points
/// </summary>
/// <param name="points">The points to check</param>
/// <returns>The smallest octant that contains all the points</returns>
Octree::Node* Octree::Node::GetContainingOctant(std::vector<DirectX::XMFLOAT3> points)
{
    // Return null if this octant doesn't contain all points
    for(DirectX::XMFLOAT3 point : points)
        if (!bounds.Contains(point))
            return nullptr;

    // Check Children
    if (HasChildren())
    {
        for (unsigned char flags = activeOctants, i = 0;
            flags > 0;
            flags >>= 1, i++)
        {
            if (flags & (1 << 0) && children[i] != nullptr) // Child exists
            {
                bool contained = true;
                // If points in child, search within that child
                for (DirectX::XMFLOAT3 point : points)
                    if (contained && !children[i]->bounds.Contains(point))
                    {
                        contained = false;
                        break;
                    }
                if(contained)
                    return children[i]->GetContainingOctant(points);
            }
        }
    }

    // Points only fit in current octant
    return this;
}

//
//  Init
//
void Octree::Node::Build()
{
    // Already built
    if (treeBuilt || treeReady)
        return;

    // Too few entities
    if (entities.size() <= 1)
    {
        treeBuilt = true;
        treeReady = true;
        return;
    }

    // Too small
    DirectX::XMFLOAT3 dim = bounds.Dimensions();
    if (dim.x < MIN_BOUNDS || dim.y < MIN_BOUNDS || dim.z < MIN_BOUNDS)
    {
        treeBuilt = true;
        treeReady = true;
        return;
    }

    // Create Octant Bounds
    AABB octantBounds[NUM_CHILDREN];
    for (int i = 0; i < NUM_CHILDREN; i++)
        octantBounds[i] = CalculateChildBounds(Octant(1 << i));

    // Determine what entities fully fit within octants
    std::vector<std::shared_ptr<Entity>> octEntities[NUM_CHILDREN];
    std::stack<int> delStack; // Stack of objects that have been placed
    for (int i=0, length=(int)entities.size(); i<length; i++)
    {
        std::shared_ptr<Entity> entityPtr = entities[i];
        for (int j = 0; j < NUM_CHILDREN; j++)
        {
            if (octantBounds[j].Contains(entityPtr->GetAABB()))
            {
                octEntities[j].push_back(entityPtr);
                delStack.push(i);
                break;
            }
        }
    }

    // Remove objects on the delStack
    while (!delStack.empty())
    {
        entities.erase(entities.begin() + delStack.top());
        delStack.pop();
    }

    // Populate Octants
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (octEntities[i].size() != 0) // Have entities to add
        {
            children[i] = new Node(octantBounds[i], octEntities[i], this);
            activeOctants ^= (1 << i);
            children[i]->Build();
        }
    }

    // Set variables
    treeBuilt = true;
    treeReady = true;
}

//
// Update
//
void Octree::Node::Update()
{
    // Add pending entities
    if (queue.size() > 0)
        ProcessPending();
    if (treeBuilt && treeReady)
    {
        // Check if this node/octant should be deleted
        if (entities.size() == 0)
        {
            if (!HasChildren())
            {
                if (currentLifespan == -1) // initial check
                    currentLifespan = MaxLifespan;
                else if (currentLifespan > 0)
                    currentLifespan--;
            }
        }
        else {
            if (currentLifespan != -1)
            {
                if (MaxLifespan <= 64)
                    MaxLifespan <<= 2; // Extend lifespan if has objects
                currentLifespan = -1;
            }
        }

        // Prune child nodes that should be deleted
        if (HasChildren())
        {
            for (unsigned char flags = activeOctants, i = 0;
                flags > 0;
                flags >>= 1, i++)
            {
                if (flags & (1 << 0) && children[i] != nullptr && children[i]->currentLifespan == 0) // Marked for deletion
                {
                    if (children[i]->entities.size() > 0) // Revive if has objects
                        children[i]->currentLifespan = -1;
                    else
                    {
                        children[i]->Clear();
                        delete children[i];
                        activeOctants ^= (1 << i);
                    }
                }
            }
        }

        // Remove Entities that don't exist anymore
        //for (int i = 0, vectorSize = (int)entities.size(); i < vectorSize; i++)
        //{
        //    // TODO 
        //    //if entity dead
        //    {
        //        entities.erase(entities.begin() + i);
        //        i--;
        //        vectorSize--;
        //    }
        //}


        // Get Moved Object that were in this node previously
        std::stack< int > movedEntities;
        for (int i = 0, vectorSize = (int)entities.size(); i < vectorSize; i++)
        {
            if (entities[i]->hasMoved)
            {
                movedEntities.push(i);
            }
        }

        // Update Child Nodes
        if (HasChildren())
        {
            for (unsigned char flags = activeOctants, i = 0;
                flags > 0;
                flags >>= 1, i++)
            {
                if (flags & (1 << 0) && children[i] != nullptr) // Active Child
                {
                    children[i]->Update();
                }
            }
        }

        // Move Moved Objects into new nodes
        std::shared_ptr<Entity> movedEntity;
        while(movedEntities.size() != 0)
        {
            movedEntity = entities[movedEntities.top()];
            Node* current = this;

            // Find region that contains this entity
            while (!current->bounds.Contains(movedEntity->GetAABB()))
            {
                if (current->parent != nullptr)
                    current = current->parent;
                else
                    break; // Root Node
            }

            // Remove Object
            entities.erase(entities.begin() + movedEntities.top());
            movedEntities.pop();

            // Insert into new node
            movedEntity->hasMoved = false;
            current->Insert(movedEntity);

            // TODO
            // Collision detection
        }
    }
}

//
//  Helper Functions
//
bool Octree::Node::Insert(std::shared_ptr<Entity> _entity)
{
    // If object doesn't fit
    if (!bounds.Contains(_entity->GetAABB()))
        return parent == nullptr ? false : parent->Insert(_entity);

    // No Other Entities or Bounds Too Small
    // Insert into this Node
    DirectX::XMFLOAT3 dim = bounds.Dimensions();
    if (entities.size() == 0 ||
        dim.x < MIN_BOUNDS ||
        dim.y < MIN_BOUNDS ||
        dim.z < MIN_BOUNDS)
    {
        entities.push_back(_entity);
        return true;
    }

    // Create Octant Bounds
    AABB octantBounds[NUM_CHILDREN];
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (children[i] != nullptr)
            octantBounds[i] = children[i]->bounds;
        else
            octantBounds[i] = CalculateChildBounds(Octant(1 << i));
    }

    // Find Octant that fits entity entirely
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (octantBounds[i].Contains(_entity->GetAABB()))
            if (children[i] != nullptr)
                return children[i]->Insert(_entity);
            else {
                // Create new Node
                children[i] = new Node(octantBounds[i], { _entity }, this);
                children[i]->Build();
                activeOctants ^= (1 << i);
                return true;
            }
    }

    // Can't fit in any child octant
    entities.push_back(_entity);
    return true;
}

AABB Octree::Node::CalculateChildBounds(Octant octant)
{
    DirectX::XMFLOAT3 center = bounds.Center();

    AABB out;
    out.min = DirectX::XMFLOAT3();
    out.max = DirectX::XMFLOAT3();
    switch (octant)
    {
    // Bottom
    case Octree::Octant::O1:        // BL
        out.min = bounds.min;       // L1
        out.max = center;           // M5
        break;
    case Octree::Octant::O2:        // TL
        out.min.x = bounds.min.x;   // L4
        out.min.y = bounds.min.y;
        out.min.z = center.z;

        out.max.x = center.x;       // M8
        out.max.y = center.y;
        out.max.z = bounds.max.z;
        break;
    case Octree::Octant::O3:        // TR
        out.min.x = center.x;       // L5
        out.min.y = bounds.min.y;
        out.min.z = center.z;

        out.max.x = bounds.max.x;   // M9
        out.max.y = center.y;
        out.max.z = bounds.max.z;
        break;
    case Octree::Octant::O4:        // BR
        out.min.x = center.x;       // L2
        out.min.y = bounds.min.y;
        out.min.z = bounds.min.z;

        out.max.x = bounds.max.x;   // M6
        out.max.y = center.y;
        out.max.z = center.z;
        break;

    // Top
    case Octree::Octant::O5:        // BL
        out.min.x = bounds.min.x;   // M1
        out.min.y = center.y;
        out.min.z = bounds.min.z;

        out.max.x = center.x;       // T5
        out.max.y = bounds.max.y;
        out.max.z = center.z;
        break;
    case Octree::Octant::O6:        // TL
        out.min.x = bounds.min.x;   // M4
        out.min.y = center.y;
        out.min.z = center.z;

        out.max.x = center.x;       // T8
        out.max.y = bounds.max.y;
        out.max.z = bounds.max.z;
        break;
    case Octree::Octant::O7:        // TR
        out.min = center;           // M5
        out.max = bounds.max;       // T9
        break;
    case Octree::Octant::O8:        // BR
        out.min.x = center.x;       // M2
        out.min.y = center.y;
        out.min.z = bounds.min.z;

        out.max.x = bounds.max.x;   // T6
        out.max.y = bounds.max.y;
        out.max.z = center.z;
        break;
    }

    return out;
}

