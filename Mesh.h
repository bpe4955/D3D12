#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include "Vertex.h"
#include "PathHelpers.h"
#include <string>

#pragma comment(lib, "assimp-vc143-mtd.lib")

class Mesh
{
public:
	Mesh(Vertex* vertices, int _vertexCount,
		unsigned int* indices, int _indexCount);
	Mesh(std::wstring relativeFilePath);
	Mesh(std::string relativeFilePath);
	Mesh(const char* relativeFilePath);
	~Mesh();
	/// <summary>
	/// Create the Vertex and Index buffers for the mesh
	/// </summary>
	/// <param name="vertices">The mesh's vertices</param>
	/// <param name="indices">The mesh's indices</param>
	void CreateBuffers(Vertex* vertices, unsigned int* indices);
	void LoadModelAssimp(std::string relativeFilePath);
	void LoadModelGiven(std::string relativeFilePath);
	/// <summary>
	/// Returns the Vertex Buffer ComPtr
	/// </summary>
	/// <returns>The Vertex Buffer ComPtr</returns>
	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexBuffer();
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	/// <summary>
	/// Returns the Index Buffer ComPtr
	/// </summary>
	/// <returns>The Index Buffer ComPtr</returns>
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexBuffer();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	/// <summary>
	/// Returns the number of indices this mesh contains
	/// </summary>
	/// <returns>The number of indices this mesh contains</returns>
	int GetIndexCount();
	/// <summary>
	/// Returns the number of vertices this mesh contains
	/// </summary>
	/// <returns>The number of vertices this mesh contains</returns>
	int GetVertexCount();
	/// <summary>
	/// Activates the buffers and draws the correct number of indices
	/// </summary>
	//void Draw();

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	int vertexCount;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
	int indexCount;
	D3D12_VERTEX_BUFFER_VIEW vbView;
	D3D12_INDEX_BUFFER_VIEW ibView;

};

