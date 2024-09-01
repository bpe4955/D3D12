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
	/*scene->mNumMeshes*/
	for (size_t meshIndex = 0; meshIndex < 1; meshIndex++) // Only read the first mesh for now
	{
		aiMesh* readMesh = scene->mMeshes[meshIndex];

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
			for (size_t f = 0; f < readMesh->mFaces[i].mNumIndices; f++)
			{
				indices.push_back(readMesh->mFaces[i].mIndices[f]);
			}
		}
	}

	aiReleaseImport(scene);

	// Create Buffers
	vertexCount = (int)vertices.size();
	indexCount = (int)indices.size();
	CreateBuffers(&vertices[0], &indices[0]);
}
