#include "Assets.h"
//#include "PathHelpers.h"
#include "D3D12Helper.h"

#define _SILENCE_FILESYSTEM_DEPRECATION_WARNING
#include <filesystem>

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <codecvt>
#include <d3dcompiler.h>


// Singleton requirement
Assets* Assets::instance;


// --------------------------------------------------------------------------
// Cleans up the asset manager and deletes any resources that are
// not stored with smart pointers.
// --------------------------------------------------------------------------
Assets::~Assets()
{
}

void Assets::Initialize(std::wstring _rootAssetPath,
	Microsoft::WRL::ComPtr<ID3D12Device> _device, bool _printLoadingProgress,
	bool _allowOnDemandLoading)
{
	rootAssetPath = _rootAssetPath;
	device = _device;
	printLoadingProgress = _printLoadingProgress;
	allowOnDemandLoading = _allowOnDemandLoading;

	// Replace all "\\" with "/" to ease lookup later
	std::replace(this->rootAssetPath.begin(), this->rootAssetPath.end(), '\\', '/');

	// Ensure the root path ends with a slash
	if (!EndsWith(rootAssetPath, L"/"))
		rootAssetPath += L"/";

	if (!allowOnDemandLoading) { LoadAllAssets(); }
}

void Assets::LoadAllAssets()
{
	if (rootAssetPath.empty()) return;

	// These files need to be loaded after all basic assets
	std::vector<std::wstring> materialPaths;

	// Recursively go through all directories starting at the root
	for (auto& item : std::filesystem::recursive_directory_iterator(FixPath(rootAssetPath)))
	{
		// Is this a regular file?
		if (item.status().type() == std::filesystem::file_type::regular)
		{
			std::wstring itemPath = item.path().wstring();

			// Replace all L"\\" with L"/" to ease lookup later
			std::replace(itemPath.begin(), itemPath.end(), '\\', '/');

			// Determine the file type
			if (EndsWith(itemPath, L".obj") || EndsWith(itemPath, L".fbx") || EndsWith(itemPath, L".dae"))
			{ LoadMesh(itemPath); }
			else if (EndsWith(itemPath, L".jpg") || EndsWith(itemPath, L".png"))
			{ LoadTexture(itemPath); }
			//else if (EndsWith(itemPath, L".dds"))
			//{ LoadDDSTexture(itemPath); }
			//else if (EndsWith(itemPath, L".spritefont"))
			//{ LoadSpriteFont(itemPath); }
			//else if (EndsWith(itemPath, L".sampler"))
			//{ LoadSampler(itemPath); }
			//else if (EndsWith(itemPath, L".material"))
			//{ materialPaths.push_back(itemPath); }
		}
	}

	// Search and load all shaders in the exe directory
	for (auto& item : std::filesystem::directory_iterator(GetFullPathTo(".")))
	{
		// Assume we're just using the filename for shaders due to being in the .exe path
		std::wstring itemPath = item.path().filename().wstring();

		// Is this a Compiled Shader Object?
		if (EndsWith(itemPath, L".cso"))
		{ LoadUnknownShader(itemPath); }
	}
}
//
//	GETTERS
//
// --------------------------------------------------------------------------
// Gets the specified mesh if it exists in the asset manager.  If on-demand
// loading is allowed, this method will attempt to load the mesh if it doesn't
// exist in the asset manager.  Otherwise, this method returns null.
//
// Notes on file names:
//  - Do include the path starting at the root asset path
//  - Do use L"/" as the folder separator
//  - Do NOT include the file extension
//  - Example: L"Models/cube"
// --------------------------------------------------------------------------
std::shared_ptr<Mesh> Assets::GetMesh(std::wstring name)
{
	// Search and return mesh if found
	auto it = meshes.find(name);
	if (it != meshes.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::wstring filePathOBJ = FixPath(rootAssetPath + name + L".obj");
		if (std::filesystem::exists(filePathOBJ)) 
		{ return LoadMesh(filePathOBJ); }
		// Is it a FBX?
		std::wstring filePathFBX = FixPath(rootAssetPath + name + L".fbx");
		if (std::filesystem::exists(filePathFBX)) { return LoadMesh(filePathFBX); }
		// Is it a FBX?
		std::wstring filePathDAE = FixPath(rootAssetPath + name + L".dae");
		if (std::filesystem::exists(filePathDAE)) { return LoadMesh(filePathDAE); }
	}

	// Unsuccessful
	return 0;
}
D3D12_CPU_DESCRIPTOR_HANDLE Assets::GetTexture(std::wstring name)
{
	// Search and return texture if found
	auto it = textures.find(name);
	if (it != textures.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// Is it a JPG?
		std::wstring filePathJPG = FixPath(rootAssetPath + name + L".jpg");
		if (std::filesystem::exists(filePathJPG)) { return LoadTexture(filePathJPG); }

		// Is it a PNG?
		std::wstring filePathPNG = FixPath(rootAssetPath + name + L".png");
		if (std::filesystem::exists(filePathPNG)) { return LoadTexture(filePathPNG); }

		// Is it a DDS?
		//std::wstring filePathDDS = FixPath(rootAssetPath + name + L".dds");
		//if (std::filesystem::exists(filePathDDS)) { return LoadDDSTexture(filePathDDS); }
	}

	// Unsuccessful
	return {};
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::GetPixelShader(std::wstring name)
{
	// Search and return shader if found
	auto it = pixelShaders.find(name);
	if (it != pixelShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(name + L".cso");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			Microsoft::WRL::ComPtr<ID3DBlob> ps = LoadPixelShader(filePath);
			return ps;
		}
	}

	// Unsuccessful
	return 0;
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::GetVertexShader(std::wstring name)
{
	// Search and return shader if found
	auto it = vertexShaders.find(name);
	if (it != vertexShaders.end())
		return it->second;

	// Attempt to load on-demand?
	if (allowOnDemandLoading)
	{
		// See if the file exists and attempt to load
		// Assume root path is same as .exe
		std::wstring filePath = FixPath(name + L".cso");
		if (std::filesystem::exists(filePath))
		{
			// Attempt to load the pixel shader and return it if successful
			Microsoft::WRL::ComPtr<ID3DBlob> vs = LoadVertexShader(filePath);
			if (vs) { return vs; }
		}
	}

	// Unsuccessful
	return 0;
}
unsigned int Assets::GetMeshCount() { return (unsigned int)meshes.size(); }
unsigned int Assets::GetTextureCount() { return (unsigned int)textures.size(); }
unsigned int Assets::GetPixelShaderCount() { return (unsigned int)pixelShaders.size(); }
unsigned int Assets::GetVertexShaderCount() { return (unsigned int)vertexShaders.size(); }
//
//	MODIFIERS
//
void Assets::AddMesh(std::wstring name, std::shared_ptr<Mesh> mesh)
{ meshes.insert({ name, mesh }); }
void Assets::AddTexture(std::wstring name, D3D12_CPU_DESCRIPTOR_HANDLE texture)
{ textures.insert({ name, texture }); }
void Assets::AddPixelShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> ps)
{ pixelShaders.insert({ name, ps }); }
void Assets::AddVertexShader(std::wstring name, Microsoft::WRL::ComPtr<ID3DBlob> vs)
{ vertexShaders.insert({ name, vs }); }
//
//	PRIVATE LOADING FUNCTIONS
//
std::shared_ptr<Mesh> Assets::LoadMesh(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading mesh: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the mesh
	std::shared_ptr<Mesh> m = std::make_shared<Mesh>(path.c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	meshes.insert({ filename, m });

	return m;
}

D3D12_CPU_DESCRIPTOR_HANDLE Assets::LoadTexture(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t assetPathLength = rootAssetPath.size();
	size_t assetPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(assetPathPosition + assetPathLength);

	if (printLoadingProgress)
	{
		printf("Loading texture: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Load the texture
	D3D12_CPU_DESCRIPTOR_HANDLE srv =
		D3D12Helper::GetInstance().LoadTexture(path.c_str());

	// Remove the file extension the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Add to the dictionary
	textures.insert({ filename, srv });
	return srv;
}

//void Assets::LoadDDSTexture(std::wstring path)
//{
//}
//
//std::shared_ptr<DirectX::SpriteFont> Assets::LoadSpriteFont(std::wstring path)
//{
//	return std::shared_ptr<DirectX::SpriteFont>();
//}
//
//void Assets::LoadSampler(std::wstring path)
//{
//}

void Assets::LoadUnknownShader(std::wstring path)
{
	// Load the file into a blob
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
	if (D3DReadFileToBlob(GetFullPathTo_Wide(path).c_str(), shaderBlob.GetAddressOf()) != S_OK)
		return;
	// Set up shader reflection to get information about
	// this shader and its variables, buffers, etc.
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl;
	D3DReflect(
		shaderBlob->GetBufferPointer(),
		shaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)refl.GetAddressOf());
	// Get the description of the shader
	D3D12_SHADER_DESC shaderDesc;
	refl->GetDesc(&shaderDesc);
	// What kind of shader?
	switch (D3D12_SHVER_GET_TYPE(shaderDesc.Version))
	{
		case D3D12_SHVER_PIXEL_SHADER: LoadPixelShader(path); break; // Load file as Pixel shader
		case D3D12_SHVER_VERTEX_SHADER: LoadVertexShader(path); break; // Load file as Vertex shader
	}
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::LoadPixelShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootAssetPath.size();
	size_t shaderPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading vertex shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	Microsoft::WRL::ComPtr<ID3DBlob> ps;
	D3DReadFileToBlob(path.c_str(), ps.GetAddressOf());

	// Success
	pixelShaders.insert({ filename, ps });
	return ps;
}
Microsoft::WRL::ComPtr<ID3DBlob> Assets::LoadVertexShader(std::wstring path)
{
	// Strip out everything before and including the asset root path
	size_t shaderPathLength = rootAssetPath.size();
	size_t shaderPathPosition = path.rfind(rootAssetPath);
	std::wstring filename = path.substr(shaderPathPosition + shaderPathLength);

	if (printLoadingProgress)
	{
		printf("Loading vertex shader: ");
		wprintf(filename.c_str());
		printf("\n");
	}

	// Remove the ".cso" from the end of the filename before using as a key
	filename = RemoveFileExtension(filename);

	// Create the simple shader and verify it actually worked
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	D3DReadFileToBlob(path.c_str(), vs.GetAddressOf());

	// Success
	vertexShaders.insert({ filename, vs });
	return vs;
}



// Helpers for determining the actual path to the executable
// Written By vixorien https://github.dev/vixorien/ggp-advanced-demos
// --------------------------------------------------------------------------
// Gets the actual path to this executable
//
// - As it turns out, the relative path for a program is different when 
//    running through VS and when running the .exe directly, which makes 
//    it a pain to properly load external files (like textures)
//    - Running through VS: Current Dir is the *project folder*
//    - Running from .exe:  Current Dir is the .exe's folder
// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
//    Visual Studio "thing", and isn't obvious unless you know to look 
//    for it.  In fact, it could be fixed by changing a setting in VS, but
//    the option is stored in a user file (.suo), which is ignored by most
//    version control packages by default.  Meaning: the option must be
//    changed every on every PC.  Ugh.  So instead, here's a helper.
// --------------------------------------------------------------------------
std::string Assets::GetExePath()
{
	// Assume the path is just the "current directory" for now
	std::string path = ".\\";

	// Get the real, full path to this executable
	char currentDir[1024] = {};
	GetModuleFileNameA(0, currentDir, 1024);

	// Find the location of the last slash charaacter
	char* lastSlash = strrchr(currentDir, '\\');
	if (lastSlash)
	{
		// End the string at the last slash character, essentially
		// chopping off the exe's file name.  Remember, c-strings
		// are null-terminated, so putting a "zero" character in 
		// there simply denotes the end of the string.
		*lastSlash = 0;

		// Set the remainder as the path
		path = currentDir;
	}

	// Toss back whatever we've found
	return path;
}
// ---------------------------------------------------
//  Same as GetExePath(), except it returns a wide character
//  string, which most of the Windows API requires.
// ---------------------------------------------------
std::wstring Assets::GetExePath_Wide()
{
	// Grab the path as a standard string
	std::string path = GetExePath();

	// Convert to a wide string
	wchar_t widePath[1024] = {};
	mbstowcs_s(0, widePath, path.c_str(), 1024);

	// Create a wstring for it and return
	return std::wstring(widePath);
}
// ----------------------------------------------------
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::string Assets::GetFullPathTo(std::string relativeFilePath)
{
	return GetExePath() + "\\" + relativeFilePath;
}
// ----------------------------------------------------
//  Same as GetFullPathTo, but with wide char strings.
// 
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::wstring Assets::GetFullPathTo_Wide(std::wstring relativeFilePath)
{
	return GetExePath_Wide() + L"\\" + relativeFilePath;
}
// ----------------------------------------------------
// Determines if the given string ends with the given ending
// ----------------------------------------------------
bool Assets::EndsWith(std::string str, std::string ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}
bool Assets::EndsWith(std::wstring str, std::wstring ending)
{
	return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}
// ----------------------------------------------------
//  Helper function for converting a wide character 
//  string to a standard ("narrow") character string
// ----------------------------------------------------
std::string Assets::ToNarrowString(const std::wstring& str)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), 0, 0, 0, 0);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], size, 0, 0);
	return result;
}
// ----------------------------------------------------
// Remove the file extension by searching for the last 
// period character and removing everything afterwards
// ----------------------------------------------------
std::wstring Assets::RemoveFileExtension(std::wstring str)
{
	size_t found = str.find_last_of('.');
	return str.substr(0, found);
}
