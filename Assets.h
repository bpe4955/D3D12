#pragma once

#include <d3d12.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <WICTextureLoader.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteFont.h>

#include "Mesh.h"
#include "Material.h"


class Assets
{
#pragma region Singleton
public:
	// Gets the one and only instance of this class
	static Assets& GetInstance()
	{
		if (!instance)
		{
			instance = new Assets();
		}

		return *instance;
	}

	// Remove these functions
	Assets(Assets const&) = delete;
	void operator=(Assets const&) = delete;

private:
	static Assets* instance;
	Assets() :
		allowOnDemandLoading(true),
		printLoadingProgress(false) {};
#pragma endregion

public:
	~Assets();
	void Initialize(
		std::wstring _rootAssetPath,
		std::wstring _rootShaderPath,
		Microsoft::WRL::ComPtr<ID3D12Device> _device,
		bool _printLoadingProgress = false,
		bool _allowOnDemandLoading = true);

	void LoadAllAssets();

	// Getters
	std::shared_ptr<Mesh> GetMesh(std::wstring name);
	D3D12_CPU_DESCRIPTOR_HANDLE GetTexture(std::wstring name, bool generateMips = true, bool isCubeMap = false);
	//std::shared_ptr<DirectX::SpriteFont> GetSpriteFont(std::wstring name);
	Microsoft::WRL::ComPtr<ID3DBlob> GetPixelShader(std::wstring name);
	Microsoft::WRL::ComPtr<ID3DBlob> GetVertexShader(std::wstring name);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSig(std::wstring name);
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPiplineState(std::wstring name);
	std::shared_ptr<Material> GetMaterial(std::wstring name);
	unsigned int GetMeshCount();
	unsigned int GetTextureCount();
	unsigned int GetRootSigCount();
	unsigned int GetPipelineStateCount();
	//unsigned int GetSpriteFontCount();
	//unsigned int GetSamplerCount();
	unsigned int GetPixelShaderCount();
	unsigned int GetVertexShaderCount();
	unsigned int GetMaterialCount();

	// Modifiers
	void AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh);
	void AddTexture(std::wstring name, D3D12_CPU_DESCRIPTOR_HANDLE texture);
	//void AddSpriteFont(std::wstring name, std::shared_ptr<DirectX::SpriteFont> font);
	void AddRootSig(std::wstring name, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig);
	void AddPipelineState(std::wstring name, Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);
	void AddPixelShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> ps);
	void AddVertexShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> vs);
	void AddMaterial(std::wstring name, std::shared_ptr<Material> material);


private:
	// Variables
	std::wstring rootAssetPath;
	std::wstring rootShaderPath;
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	bool printLoadingProgress;
	bool allowOnDemandLoading;

	std::unordered_map<std::wstring, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<std::wstring, D3D12_CPU_DESCRIPTOR_HANDLE> textures;
	//std::unordered_map<std::wstring, std::shared_ptr<DirectX::SpriteFont>> spriteFonts;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSigs;
	bool inputElementsCreated = false;
	static const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> pixelShaders;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> vertexShaders;
	std::unordered_map<std::wstring, std::shared_ptr<Material>> materials;

	///
	/// Loading Functions
	/// 
	std::shared_ptr<Mesh> LoadMesh(std::wstring path);
	D3D12_CPU_DESCRIPTOR_HANDLE LoadTexture(std::wstring path);
	D3D12_CPU_DESCRIPTOR_HANDLE LoadDDSTexture(std::wstring path, bool generateMips = true, bool isCubeMap = false);
	//D3D12_GPU_DESCRIPTOR_HANDLE LoadTexCubeDEPRECATED(std::wstring path);
	/*Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>*/ 
	//std::shared_ptr<DirectX::SpriteFont> LoadSpriteFont(std::wstring path);
	Microsoft::WRL::ComPtr<ID3D12RootSignature> LoadRootSig(std::wstring path);
	Microsoft::WRL::ComPtr<ID3D12PipelineState> LoadPipelineState(std::wstring path);
	void LoadUnknownShader(std::wstring path);
	Microsoft::WRL::ComPtr<ID3DBlob> LoadPixelShader(std::wstring path);
	Microsoft::WRL::ComPtr<ID3DBlob> LoadVertexShader(std::wstring path);
	std::shared_ptr<Material> LoadMaterial(std::wstring path);

	/// Helpers for determining the actual path to the executable
	/// Written By vixorien https://github.dev/vixorien/ggp-advanced-demos
	std::string GetExePath();
	std::wstring GetExePath_Wide();
	std::string GetFullPathTo(std::string relativeFilePath);
	std::wstring GetFullPathTo_Wide(std::wstring relativeFilePath);
	bool EndsWith(std::string str, std::string ending);
	bool EndsWith(std::wstring str, std::wstring ending);
	std::string ToNarrowString(const std::wstring& str);
	std::wstring RemoveFileExtension(std::wstring str);
};

