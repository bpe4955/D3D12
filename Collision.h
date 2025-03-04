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

	bool Overlaps(AABB other)
	{
		return
			this->min.x <= other.max.x &&
			this->max.x >= other.min.x &&
			this->min.y <= other.max.y &&
			this->max.y >= other.min.y &&
			this->min.z <= other.max.z &&
			this->max.z >= other.min.z;
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
		return plane.x * pt.x + plane.y * pt.y + plane.z * pt.z + plane.w;
	}

	bool Overlaps(AABB other)
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
