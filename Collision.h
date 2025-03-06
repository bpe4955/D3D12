#pragma once
#include <DirectXMath.h>

inline float Dot(DirectX::XMFLOAT3 vec1, DirectX::XMFLOAT3 vec2)
{
	DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&vec1);
	DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&vec2);

	float ans;
	DirectX::XMStoreFloat(&ans,
		DirectX::XMVector3Dot(a, b));

	return ans;
}
inline float Dot(DirectX::XMFLOAT4 vec1, DirectX::XMFLOAT3 vec2)
{
	DirectX::XMFLOAT3 temp = DirectX::XMFLOAT3(vec1.x, vec1.y, vec1.z);
	DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&temp);
	DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&vec2);

	float ans;
	DirectX::XMStoreFloat(&ans,
		DirectX::XMVector3Dot(a, b));

	return ans;
}
inline float CalcD(DirectX::XMFLOAT4 vec1, DirectX::XMFLOAT3 vec2)
{
	return
		vec1.x * vec2.x +
		vec1.y * vec2.y +
		vec1.z * vec2.z;
}

struct Ray
{
	DirectX::XMFLOAT3 origin;
	DirectX::XMFLOAT3 direction;
};

struct AABB
{
	DirectX::XMFLOAT3 max;
	DirectX::XMFLOAT3 min;

	bool Contains(DirectX::XMFLOAT3 point)
	{
		return
			point.x >= this->min.x &&
			point.x <= this->max.x &&
			point.y >= this->min.y &&
			point.y <= this->max.y &&
			point.z >= this->min.z &&
			point.z <= this->max.z;

	}

	bool Contains(AABB other)
	{
		return
			this->min.x <= other.min.x &&
			this->max.x >= other.max.x &&
			this->min.y <= other.min.y &&
			this->max.y >= other.max.y &&
			this->min.z <= other.min.z &&
			this->max.z >= other.max.z;

	}

	// https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
	bool Intersects(Ray other)
	{
		DirectX::XMFLOAT3 tMin = DirectX::XMFLOAT3(
			(this->min.x - other.origin.x) / other.direction.x,
			(this->min.y - other.origin.y) / other.direction.y,
			(this->min.z - other.origin.z) / other.direction.z
		);
		DirectX::XMFLOAT3 tMax = DirectX::XMFLOAT3(
			(this->max.x - other.origin.x) / other.direction.x,
			(this->max.y - other.origin.y) / other.direction.y,
			(this->max.z - other.origin.z) / other.direction.z
		);
		DirectX::XMFLOAT3 t1 = DirectX::XMFLOAT3();
		t1.x = (((tMin.x) < (tMax.x)) ? (tMin.x) : (tMax.x));
		t1.y = (((tMin.y) < (tMax.y)) ? (tMin.y) : (tMax.y));
		t1.z = (((tMin.z) < (tMax.z)) ? (tMin.z) : (tMax.z));
		
		DirectX::XMFLOAT3 t2 = DirectX::XMFLOAT3();
		t2.x = (((tMin.x) > (tMax.x)) ? (tMin.x) : (tMax.x));
		t2.y = (((tMin.y) > (tMax.y)) ? (tMin.y) : (tMax.y));
		t2.z = (((tMin.z) > (tMax.z)) ? (tMin.z) : (tMax.z));

		float temp = (((t1.x) > (t1.y)) ? (t1.x) : (t1.y));
		float tNear = (((temp) > (t1.z)) ? (temp) : (t1.z));
		temp = (((t2.x) < (t2.y)) ? (t2.x) : (t2.y));
		float tFar = (((temp) < (t2.z)) ? (temp) : (t2.z));

		return tNear < tFar;
	}

	// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
	bool IntersectsPlane(DirectX::XMFLOAT4 normal)
	{
		// Convert AABB to center-extents representation
		DirectX::XMFLOAT3 c = DirectX::XMFLOAT3(		// Compute AABB center
			(max.x + min.x) * 0.5f,
			(max.y + min.y) * 0.5f,
			(max.z + min.z) * 0.5f
		);
			
		DirectX::XMFLOAT3 e = DirectX::XMFLOAT3(		// Compute positive extents
			max.x - c.x,
			max.y - c.y,
			max.z - c.z
		);

		// Compute the projection interval radius of aabb onto L(t) = c + t * n
		float r = 
			e.x*std::abs(normal.x) + 
			e.y*std::abs(normal.y) + 
			e.z*std::abs(normal.z);

		// Compute distance of box center from plane
		float dist = Dot(DirectX::XMFLOAT3(normal.x, normal.y, normal.z), c) - normal.w;

		// Intersection occurs when distance s falls within [-r,+r] interval
		if (std::abs(dist) <= r)
			return true;

		// Within Half-space
		return dist > r;
	}

	DirectX::XMFLOAT3 Dimensions()
	{
		return DirectX::XMFLOAT3(
			this->max.x - this->min.x,
			this->max.y - this->min.y,
			this->max.z - this->min.z
		);
	}

	DirectX::XMFLOAT3 Center()
	{
		return DirectX::XMFLOAT3(
			(this->min.x + this->max.x) / 2,
			(this->min.y + this->max.y) / 2,
			(this->min.z + this->max.z) / 2
		);
	}
};

// https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
// https://iquilezles.org/articles/frustumcorrect/
struct Frustum
{
	DirectX::XMFLOAT4 normals[6];
	DirectX::XMFLOAT3 points[8];

	// https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
	float DistanceToPoint(DirectX::XMFLOAT4 plane, DirectX::XMFLOAT3 pt)
	{
		//  0 ~ point on plane
		// >0 ~ point in front of plane
		// <0 ~ point behind plane
		return 
			plane.x * pt.x + 
			plane.y * pt.y + 
			plane.z * pt.z + 
			plane.w;
	}

	bool Intersects(AABB other)
	{
		DirectX::XMFLOAT3 p = other.min;

		for (int i = 0; i < 6; i++)
		{
			DirectX::XMFLOAT3 p = other.min;
			if (normals[i].x >= 0)
				p.x = other.max.x;
			if (normals[i].y >= 0)
				p.y = other.max.y;
			if (normals[i].z >= 0)
				p.z = other.max.z;

			float r = std::abs((other.max.x - other.min.x) * 0.5f * normals[i].x)
				+ std::abs((other.max.y - other.min.y) * 0.5f * normals[i].y)
				+ std::abs((other.max.z - other.min.z) * 0.5f * normals[i].z);

			float dist = DistanceToPoint(normals[i], p);

			if (dist >= -r)
				return false;
		}
		return true;


		// check box outside/inside of frustum
		for (int i = 0; i < 6; i++)
		{
			int out = 0;
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.min.x, other.min.y, other.min.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.max.x, other.min.y, other.min.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.min.x, other.max.y, other.min.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.max.x, other.max.y, other.min.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.min.x, other.min.y, other.max.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.max.x, other.min.y, other.max.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.min.x, other.max.y, other.max.z)) < 0.0) ? 1 : 0);
			out += ((DistanceToPoint(normals[i], DirectX::XMFLOAT3(other.max.x, other.max.y, other.max.z)) < 0.0) ? 1 : 0);
			if (out == 8) return false; // All corners are out
		}

		// check frustum outside/inside box
		//int out;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > other.max.x) ? 1 : 0); if (out == 8) return false;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < other.min.x) ? 1 : 0); if (out == 8) return false;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > other.max.y) ? 1 : 0); if (out == 8) return false;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < other.min.y) ? 1 : 0); if (out == 8) return false;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > other.max.z) ? 1 : 0); if (out == 8) return false;
		//out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < other.min.z) ? 1 : 0); if (out == 8) return false;

		return true;
	}
};
