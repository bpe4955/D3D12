#pragma once
#include "BufferStructs.h"
#include "dxcore.h"
#include <vector>
#include "Entity.h"
#include "Window.h"
#include "Camera.h"

// https://github.dev/d3dcoder/d3d12book/tree/master/Chapter%2020%20Shadow%20Mapping/Shadows

class ShadowLight
{
public:
	/// <summary>
	/// Shadow Light constructor for a directional light
	/// </summary>
	ShadowLight(DirectX::XMFLOAT3 _direction, float _intensity, DirectX::XMFLOAT3 _color);
	/// <summary>
	/// Shadow Light constructor for a spot light
	/// </summary>
	ShadowLight(DirectX::XMFLOAT3 _direction, DirectX::XMFLOAT3 _position,
		float _range, float _intensity, float _spotFalloff,
		DirectX::XMFLOAT3 _color);
	/// <summary>
	/// Shadow Light constructor for a given light
	/// </summary>
	/// <param name="_light"></param>
	ShadowLight(Light _light);
	~ShadowLight();

	// Getters
	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShadowSRV();
	//Microsoft::WRL::ComPtr<ID3D11DepthStencilView> GetShadowDSV();
	int GetResolution();
	int GetType();
	DirectX::XMFLOAT3 GetDirection();
	DirectX::XMFLOAT3 GetPosition();
	DirectX::XMFLOAT4X4 GetView();
	DirectX::XMFLOAT4X4 GetProjection();
	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle();
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDSVHeap();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSRVHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSRVHandle();
	unsigned int GetSRVDescriptorOffset();

	// Setters
	void SetLightProjectionSize(float _lightProjectionSize);
	void SetType(int type);
	void SetFov(float _fov);
	void SetDirection(DirectX::XMFLOAT3 _direction);
	void SetPosition(DirectX::XMFLOAT3 _position);
	void SetGPUSRVHandle(D3D12_GPU_DESCRIPTOR_HANDLE  _gpuSrv);
	void SetSRVDescriptorOffset(unsigned int _srvDescriptorOffset);

	// Public Functions
	//void Update(std::vector<Entity> entities, Microsoft::WRL::ComPtr<ID3D12RenderTargetView> _backBufferRTV,
	//	Microsoft::WRL::ComPtr<ID3D12DepthStencilView> _depthBufferDSV);

private:
	Light light;
	int shadowMapResolution;
	float lightProjectionSize;

	// View Projection
	DirectX::XMFLOAT4X4 viewMatrix;
	bool dirtyView;
	DirectX::XMFLOAT4X4 projMatrix;
	bool dirtyProjection;
	float fov;

	// Resources
	//static inline const unsigned int HeapSize = 5;
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowMap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDsv;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv;
	unsigned int srvDescriptorOffset = 0;

	// Private Functions
	void Init();
	void CreateShadowMapData();
	void UpdateViewMatrix();
	void UpdateProjectionMatrix();
	void UpdateFrustum();
};

