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
		std::wstring rootAssetPath,
		Microsoft::WRL::ComPtr<ID3D12Device> _device,
		bool _printLoadingProgress = false,
		bool _allowOnDemandLoading = true);

	void LoadAllAssets();

	// Getters
	std::shared_ptr<Mesh> GetMesh(std::wstring name);
	D3D12_CPU_DESCRIPTOR_HANDLE GetTexture(std::wstring name);
	//std::shared_ptr<Material> GetMaterial(std::wstring name);
	//std::shared_ptr<DirectX::SpriteFont> GetSpriteFont(std::wstring name);
	//Microsoft::WRL::ComPtr<ID3D11SamplerState> GetSampler(std::wstring name);
	Microsoft::WRL::ComPtr<ID3DBlob> GetPixelShader(std::wstring name);
	Microsoft::WRL::ComPtr<ID3DBlob> GetVertexShader(std::wstring name);
	unsigned int GetMeshCount();
	unsigned int GetTextureCount();
	//unsigned int GetMaterialCount();
	//unsigned int GetSpriteFontCount();
	//unsigned int GetSamplerCount();
	unsigned int GetPixelShaderCount();
	unsigned int GetVertexShaderCount();

	// Modifiers
	void AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh);
	void AddTexture(std::wstring name, D3D12_CPU_DESCRIPTOR_HANDLE texture);
	//void AddMaterial(std::wstring name, std::shared_ptr<Material> material);
	//void AddSpriteFont(std::wstring name, std::shared_ptr<DirectX::SpriteFont> font);
	//void AddSampler(std::wstring name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);
	void AddPixelShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> ps);
	void AddVertexShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> vs);


private:
	// Variables
	std::wstring rootAssetPath;
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	bool printLoadingProgress;
	bool allowOnDemandLoading;

	std::unordered_map<std::wstring, std::shared_ptr<Mesh>> meshes;
	std::unordered_map<std::wstring, D3D12_CPU_DESCRIPTOR_HANDLE> textures;
	//std::unordered_map<std::wstring, std::shared_ptr<Material>> materials;
	//std::unordered_map<std::wstring, std::shared_ptr<DirectX::SpriteFont>> spriteFonts;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> pixelShaders;
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> vertexShaders;
	//std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers;

	///
	/// Loading Functions
	/// 
	std::shared_ptr<Mesh> LoadMesh(std::wstring path);
	D3D12_CPU_DESCRIPTOR_HANDLE LoadTexture(std::wstring path);
	/*Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>*/ 
	//void LoadDDSTexture(std::wstring path);
	//std::shared_ptr<DirectX::SpriteFont> LoadSpriteFont(std::wstring path);
	/*Microsoft::WRL::ComPtr<ID3D11SamplerState>*/
	//void LoadSampler(std::wstring path);
	//std::shared_ptr<Material> LoadMaterial(std::wstring path);
	void LoadUnknownShader(std::wstring path);
	Microsoft::WRL::ComPtr<ID3DBlob> LoadPixelShader(std::wstring path);
	Microsoft::WRL::ComPtr<ID3DBlob> LoadVertexShader(std::wstring path);

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

