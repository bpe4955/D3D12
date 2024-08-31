#include "Mesh.h"
#include <iostream>
#include <fstream>
#include <vector>
#include "Graphics.h"
#include "D3D12Helper.h"

// Manual assimp install
// https://github.com/assimp/assimp/blob/master/Build.md
// Might need to run bootstrap-vcpkg.bat if cannot detect ./vcpkg command
// git clone http://github.com/Microsoft/vcpkg.git
// cd vcpkg
// ./bootstrap-vcpkg.sh
// ./vcpkg integrate install
// ./vcpkg install assimp							(Auto done by Visual Studio)

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
//#include "assimp/material.h"

using namespace DirectX;

// Constructors
Mesh::Mesh(Vertex* vertices, int _vertexCount, unsigned int* indices, int _indexCount) :
	vertexCount(_vertexCount),
	indexCount(_indexCount)

{
	CreateBuffers(vertices, indices);
}

Mesh::Mesh(std::wstring relativeFilePath) :
	vertexCount(0),
	indexCount(0)
{
	//LoadModelGiven(WideToNarrow(relativeFilePath));
	LoadModelAssimp(WideToNarrow(relativeFilePath));
}

Mesh::Mesh(std::string relativeFilePath) :
	vertexCount(0),
	indexCount(0)
{
	//LoadModelGiven(relativeFilePath);
	LoadModelAssimp(relativeFilePath);
}

Mesh::Mesh(const char* relativeFilePath) :
	vertexCount(0),
	indexCount(0)
{
	//LoadModelGiven(std::string(relativeFilePath));
	LoadModelAssimp(std::string(relativeFilePath));
}

Mesh::~Mesh() {}

//Getters
Microsoft::WRL::ComPtr<ID3D12Resource> Mesh::GetVertexBuffer() { return vertexBuffer; }
D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() { return vbView; }
Microsoft::WRL::ComPtr<ID3D12Resource> Mesh::GetIndexBuffer() { return indexBuffer; }
D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() { return ibView; }
int Mesh::GetIndexCount() { return indexCount; }
int Mesh::GetVertexCount() { return vertexCount; }

void Mesh::CreateBuffers(Vertex* vertices, unsigned int* indices)
{
	D3D12Helper& dx12Helper = D3D12Helper::GetInstance();
	vertexBuffer = dx12Helper.CreateStaticBuffer(sizeof(Vertex), vertexCount, vertices);
	vbView = {};
	vbView.StrideInBytes = sizeof(Vertex);
	vbView.SizeInBytes = sizeof(Vertex) * vertexCount;
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();

	indexBuffer = dx12Helper.CreateStaticBuffer(sizeof(unsigned int), indexCount, indices);
	ibView = {};
	ibView.Format = DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = sizeof(unsigned int) * indexCount;
	ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
}

// Helper Functions
void Mesh::LoadModelAssimp(std::string fileName)
{
	const aiScene* scene = aiImportFile(fileName.c_str(), aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_ConvertToLeftHanded);

	if (!scene) {
		std::cerr << "Could not load file " << fileName << ": " << aiGetErrorString() << std::endl;
		return;
	}

	// Load all meshes (assimp separates a model into a mesh for each material)
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* readMesh = scene->mMeshes[i];

		// Material Data
		//aiMaterial* material = scene->mMaterials[readMesh->mMaterialIndex];
		//aiColor4D specularColor;
		//aiColor4D diffuseColor;
		//aiColor4D ambientColor;
		//float shininess;
		//
		//aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &specularColor);
		//aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuseColor);
		//aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &ambientColor);
		//aiGetMaterialFloat(material, AI_MATKEY_SHININESS, &shininess);

		// Vertex Data
		if (readMesh->HasPositions() && readMesh->HasNormals())
		{
			for (size_t i = 0; i < readMesh->mNumVertices; i++)
			{
				Vertex v;

				aiVector3D pos = readMesh->mVertices[i];
				v.Position.x = pos.x;
				v.Position.y = pos.y;
				v.Position.z = pos.z;

				aiVector3D norm = readMesh->mNormals[i];
				v.Normal.x = norm.x;
				v.Normal.y = norm.y;
				v.Normal.z = norm.z;

				if (readMesh->HasTextureCoords(0)) {
					aiVector3D uv = readMesh->mTextureCoords[0][i];
					v.UV.x = uv.x;
					v.UV.y = uv.y;
				}

				if (readMesh->HasTangentsAndBitangents()) {
					aiVector3D tan = readMesh->mTangents[i];
					v.Tangent.x = tan.x;
					v.Tangent.y = tan.y;
					v.Tangent.z = tan.z;
				}

				vertices.push_back(v);
			}
		}

		// Indices
		//std::vector<unsigned int> indices;
		//indices.reserve(readMesh->mNumFaces * 3);
		for (size_t i = 0; i < readMesh->mNumFaces; i++)
		{
			//assert(readMesh->mFaces[i].mNumIndices == 3);

			indices.push_back(readMesh->mFaces[i].mIndices[0]);
			indices.push_back(readMesh->mFaces[i].mIndices[1]);
			indices.push_back(readMesh->mFaces[i].mIndices[2]);
		}
	}

	aiReleaseImport(scene);

	// Create Buffers
	vertexCount = (int)vertices.size();
	indexCount = (int)indices.size();
	CreateBuffers(&vertices[0], &indices[0]);
}
void Mesh::LoadModelGiven(std::string relativeFilePath) {
	// Author: Chris Cascioli
	// Purpose: Basic .OBJ 3D model loading, supporting positions, uvs and normals
	// 
	// - You are allowed to directly copy/paste this into your code base
	//   for assignments, given that you clearly cite that this is not
	//   code of your own design.
	//
	// - NOTE: You'll need to #include <fstream>


	// File input object
	std::ifstream obj(relativeFilePath);

	// Check for successful open
	if (!obj.is_open())
		return;

	// Variables used while reading the file
	std::vector<XMFLOAT3> positions;	// Positions from the file
	std::vector<XMFLOAT3> normals;		// Normals from the file
	std::vector<XMFLOAT2> uvs;		// UVs from the file
	std::vector<Vertex> verts;		// Verts we're assembling
	std::vector<UINT> indices;		// Indices of these verts
	int vertCounter = 0;			// Count of vertices
	int indexCounter = 0;			// Count of indices
	char chars[100];			// String for line reading

	// Still have data left?
	while (obj.good())
	{
		// Get the line (100 characters should be more than enough)
		obj.getline(chars, 100);

		// Check the type of line
		if (chars[0] == 'v' && chars[1] == 'n')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 norm;
			sscanf_s(
				chars,
				"vn %f %f %f",
				&norm.x, &norm.y, &norm.z);

			// Add to the list of normals
			normals.push_back(norm);
		}
		else if (chars[0] == 'v' && chars[1] == 't')
		{
			// Read the 2 numbers directly into an XMFLOAT2
			XMFLOAT2 uv;
			sscanf_s(
				chars,
				"vt %f %f",
				&uv.x, &uv.y);

			// Add to the list of uv's
			uvs.push_back(uv);
		}
		else if (chars[0] == 'v')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 pos;
			sscanf_s(
				chars,
				"v %f %f %f",
				&pos.x, &pos.y, &pos.z);

			// Add to the positions
			positions.push_back(pos);
		}
		else if (chars[0] == 'f')
		{
			// Read the face indices into an array
			// NOTE: This assumes the given obj file contains
			//  vertex positions, uv coordinates AND normals.
			unsigned int i[12];
			int numbersRead = sscanf_s(
				chars,
				"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
				&i[0], &i[1], &i[2],
				&i[3], &i[4], &i[5],
				&i[6], &i[7], &i[8],
				&i[9], &i[10], &i[11]);

			// If we only got the first number, chances are the OBJ
			// file has no UV coordinates.  This isn't great, but we
			// still want to load the model without crashing, so we
			// need to re-read a different pattern (in which we assume
			// there are no UVs denoted for any of the vertices)
			if (numbersRead == 1)
			{
				// Re-read with a different pattern
				numbersRead = sscanf_s(
					chars,
					"f %d//%d %d//%d %d//%d %d//%d",
					&i[0], &i[2],
					&i[3], &i[5],
					&i[6], &i[8],
					&i[9], &i[11]);

				// The following indices are where the UVs should 
				// have been, so give them a valid value
				i[1] = 1;
				i[4] = 1;
				i[7] = 1;
				i[10] = 1;

				// If we have no UVs, create a single UV coordinate
				// that will be used for all vertices
				if (uvs.size() == 0)
					uvs.push_back(XMFLOAT2(0, 0));
			}

			// - Create the verts by looking up
			//    corresponding data from vectors
			// - OBJ File indices are 1-based, so
			//    they need to be adusted
			Vertex v1;
			v1.Position = positions[i[0] - 1];
			v1.UV = uvs[i[1] - 1];
			v1.Normal = normals[i[2] - 1];

			Vertex v2;
			v2.Position = positions[i[3] - 1];
			v2.UV = uvs[i[4] - 1];
			v2.Normal = normals[i[5] - 1];

			Vertex v3;
			v3.Position = positions[i[6] - 1];
			v3.UV = uvs[i[7] - 1];
			v3.Normal = normals[i[8] - 1];

			// The model is most likely in a right-handed space,
			// especially if it came from Maya.  We want to convert
			// to a left-handed space for DirectX.  This means we 
			// need to:
			//  - Invert the Z position
			//  - Invert the normal's Z
			//  - Flip the winding order
			// We also need to flip the UV coordinate since DirectX
			// defines (0,0) as the top left of the texture, and many
			// 3D modeling packages use the bottom left as (0,0)

			// Flip the UV's since they're probably "upside down"
			v1.UV.y = 1.0f - v1.UV.y;
			v2.UV.y = 1.0f - v2.UV.y;
			v3.UV.y = 1.0f - v3.UV.y;

			// Flip Z (LH vs. RH)
			v1.Position.z *= -1.0f;
			v2.Position.z *= -1.0f;
			v3.Position.z *= -1.0f;

			// Flip normal's Z
			v1.Normal.z *= -1.0f;
			v2.Normal.z *= -1.0f;
			v3.Normal.z *= -1.0f;

			// Add the verts to the vector (flipping the winding order)
			verts.push_back(v1);
			verts.push_back(v3);
			verts.push_back(v2);
			vertCounter += 3;

			// Add three more indices
			indices.push_back(indexCounter); indexCounter += 1;
			indices.push_back(indexCounter); indexCounter += 1;
			indices.push_back(indexCounter); indexCounter += 1;

			// Was there a 4th face?
			// - 12 numbers read means 4 faces WITH uv's
			// - 8 numbers read means 4 faces WITHOUT uv's
			if (numbersRead == 12 || numbersRead == 8)
			{
				// Make the last vertex
				Vertex v4;
				v4.Position = positions[i[9] - 1];
				v4.UV = uvs[i[10] - 1];
				v4.Normal = normals[i[11] - 1];

				// Flip the UV, Z pos and normal's Z
				v4.UV.y = 1.0f - v4.UV.y;
				v4.Position.z *= -1.0f;
				v4.Normal.z *= -1.0f;

				// Add a whole triangle (flipping the winding order)
				verts.push_back(v1);
				verts.push_back(v4);
				verts.push_back(v3);
				vertCounter += 3;

				// Add three more indices
				indices.push_back(indexCounter); indexCounter += 1;
				indices.push_back(indexCounter); indexCounter += 1;
				indices.push_back(indexCounter); indexCounter += 1;
			}
		}
	}

	// Close the file and create the actual buffers
	obj.close();

	// - At this point, "verts" is a vector of Vertex structs, and can be used
	//    directly to create a vertex buffer:  &verts[0] is the address of the first vert
	//
	// - The vector "indices" is similar. It's a vector of unsigned ints and
	//    can be used directly for the index buffer: &indices[0] is the address of the first int
	//
	// - "vertCounter" is the number of vertices
	// - "indexCounter" is the number of indices
	// - Yes, these are effectively the same since OBJs do not index entire vertices!  This means
	//    an index buffer isn't doing much for us.  We could try to optimize the mesh ourselves
	//    and detect duplicate vertices, but at that point it would be better to use a more
	//    sophisticated model loading library like TinyOBJLoader or The Open Asset Importer Library
	vertexCount = vertCounter;
	indexCount = indexCounter;
	CreateBuffers(&verts[0], &indices[0]);
}

