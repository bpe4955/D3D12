#include "ShadowLight.h"
#include "math.h"
#include "Graphics.h"

//
//  Constructors
//
ShadowLight::ShadowLight(DirectX::XMFLOAT3 _direction, float _intensity, DirectX::XMFLOAT3 _color)
{
    light = {};
    light.Type = LIGHT_TYPE_DIRECTIONAL;
    light.Direction = _direction;
    light.Intensity = _intensity;
    light.Color = _color;

    Init();
}

ShadowLight::ShadowLight(DirectX::XMFLOAT3 _direction, DirectX::XMFLOAT3 _position, float _range, float _intensity, float _spotFalloff, DirectX::XMFLOAT3 _color)
{
    light = {};
    light.Type = LIGHT_TYPE_SPOT;
    light.Direction = _direction;
    light.Position = _position;
    light.Range = _range;
    light.Intensity = _intensity;
    light.SpotFalloff = _spotFalloff;
    light.Color = _color;

    Init();
}

ShadowLight::ShadowLight(Light _light)
{
    light = _light;

    Init();
}

ShadowLight::~ShadowLight()
{
    // Will want to remove data from D3D12Helper DescriptorHeap as to not fill up with unreferenced pointers
}


//
//  Getters
//
DirectX::XMFLOAT4X4 ShadowLight::GetView()
{
    if (dirtyView)
        UpdateViewMatrix();
    return viewMatrix;
}
DirectX::XMFLOAT4X4 ShadowLight::GetProjection()
{
    if (dirtyProjection)
        UpdateProjectionMatrix();
    return projMatrix;
}
Frustum ShadowLight::GetFrustum()
{
    if(dirtyFrustum)
        UpdateFrustum();
    return frustum;
}
Light ShadowLight::GetLight() { return light; }
int ShadowLight::GetResolution() { return shadowMapResolution; }
int ShadowLight::GetType() { return light.Type; }
DirectX::XMFLOAT3 ShadowLight::GetDirection() { return light.Direction; }
DirectX::XMFLOAT3 ShadowLight::GetPosition() { return light.Position; }
Microsoft::WRL::ComPtr<ID3D12Resource> ShadowLight::GetResource() { return shadowMap; }
D3D12_CPU_DESCRIPTOR_HANDLE ShadowLight::GetDSVHandle() { return cpuDsv; }
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ShadowLight::GetDSVHeap() { return dsvHeap; }
D3D12_CPU_DESCRIPTOR_HANDLE ShadowLight::GetCPUSRVHandle() { return cpuSrv; }
D3D12_GPU_DESCRIPTOR_HANDLE ShadowLight::GetGPUSRVHandle() { return gpuSrv; }
unsigned int ShadowLight::GetSRVDescriptorOffset() { return srvDescriptorOffset; }

//
//  Setters
//
void ShadowLight::SetLightProjectionSize(float _lightProjectionSize)
{
    lightProjectionSize = _lightProjectionSize;
    dirtyProjection = true;
    dirtyFrustum = true;
}
void ShadowLight::SetType(int type)
{
    light.Type = type;
    dirtyProjection = true;
    dirtyFrustum = true;
}
void ShadowLight::SetFov(float _fov)
{
    fov = _fov;
    dirtyProjection = true;
    dirtyFrustum = true;
}
void ShadowLight::SetDirection(DirectX::XMFLOAT3 _direction)
{
    light.Direction = _direction;
    dirtyView = true;
    dirtyFrustum = true;
}
void ShadowLight::SetPosition(DirectX::XMFLOAT3 _position)
{
    light.Position = _position;
    dirtyView = true;
    dirtyFrustum = true;
}

void ShadowLight::SetGPUSRVHandle(D3D12_GPU_DESCRIPTOR_HANDLE _gpuSrv) { gpuSrv = _gpuSrv; }

void ShadowLight::SetSRVDescriptorOffset(unsigned int _srvDescriptorOffset) { srvDescriptorOffset = _srvDescriptorOffset; }

//
//  Private Functions
//
void ShadowLight::Init()
{
    // View Projection Matrices
    dirtyProjection = true;
    dirtyView = true;
    dirtyFrustum = true;
    lightProjectionSize = 30.f;


    nearClip = 0.05f;
    if (light.Type == LIGHT_TYPE_DIRECTIONAL)
        farClip = 40.f;
    else
        farClip = light.Range * 1.1f;

    UpdateProjectionMatrix();
    UpdateViewMatrix();

    // Buffer Data
    shadowMapResolution = 1024;
    CreateShadowMapData();
}

void ShadowLight::CreateShadowMapData()
{
    // Create texture that will be the shadow map
    UINT16 arrySize = light.Type == LIGHT_TYPE_POINT ? 6 : 1;
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0; 
    texDesc.Width = shadowMapResolution;
    texDesc.Height = shadowMapResolution;
    texDesc.DepthOrArraySize = arrySize;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS /*DXGI_FORMAT_R32_TYPELESS*/;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear = {};
    ZeroMemory(&optClear, sizeof(D3D12_CLEAR_VALUE));
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProps = {};
    ZeroMemory(&heapProps, sizeof(D3D12_HEAP_PROPERTIES));
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.VisibleNodeMask = 1;

    Graphics::Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&shadowMap));

    assert(shadowMap);

    // Create Depth Stencil View
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    ZeroMemory(&dsvDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    ZeroMemory(&dsvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    Graphics::Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf()));

    cpuDsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

    Graphics::Device->CreateDepthStencilView(
        shadowMap.Get(),
        &dsvDesc, 
        cpuDsv);

    assert(cpuDsv.ptr);

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    ZeroMemory(&srvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Non-shader visible for CPU-side-only descriptor heap!
    srvHeapDesc.NodeMask = 0;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    Graphics::Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf()));

    Graphics::Device->CreateShaderResourceView(
        shadowMap.Get(), 
        &srvDesc, 
        srvHeap->GetCPUDescriptorHandleForHeapStart());

    cpuSrv = srvHeap->GetCPUDescriptorHandleForHeapStart();

    assert(cpuSrv.ptr);

    gpuSrv = {};
}

void ShadowLight::UpdateViewMatrix()
{
    DirectX::XMVECTOR lightDirection;
    DirectX::XMVECTOR lightPosition;
    lightDirection = DirectX::XMLoadFloat3(&light.Direction);
    lightPosition = DirectX::XMLoadFloat3(&light.Position);
    switch (light.Type)
    {
    case LIGHT_TYPE_DIRECTIONAL:
        // Only needs to update if direction changes
        DirectX::XMMATRIX lightView = DirectX::XMMatrixLookToLH(
            DirectX::XMVectorAdd(DirectX::XMVectorScale(lightDirection, farClip * -0.5f), // Position: "Backing up" 20 units from desired center
                lightPosition), 
            lightDirection, // Direction: light's direction
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)); // Up: World up vector (Y axis)
        XMStoreFloat4x4(&viewMatrix, lightView);
        break;
    case LIGHT_TYPE_SPOT:
        // Only needs to update if direction or position changes
        lightView = DirectX::XMMatrixLookToLH(
            lightPosition, // Position
            lightDirection, // Direction: light's direction
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)); // Up: World up vector (Y axis)
        XMStoreFloat4x4(&viewMatrix, lightView);
        break;
    }

    dirtyView = false;
}

void ShadowLight::UpdateProjectionMatrix()
{
    switch (light.Type)
    {
    case LIGHT_TYPE_DIRECTIONAL:
        DirectX::XMMATRIX lightProjection = DirectX::XMMatrixOrthographicLH(
            lightProjectionSize,
            lightProjectionSize,
            nearClip,
            farClip);
        XMStoreFloat4x4(&projMatrix, lightProjection);
        break;
    case LIGHT_TYPE_SPOT:
        fov = DirectX::XM_PI / (sqrt(light.SpotFalloff));
        lightProjection = DirectX::XMMatrixPerspectiveFovLH(
            fov,
            1.0f,
            nearClip,
            farClip);
        XMStoreFloat4x4(&projMatrix, lightProjection);
        break;
    }

    dirtyProjection = false;
}

void ShadowLight::UpdateFrustum()
{
    // Variables
    float halfFarLength = light.Type == LIGHT_TYPE_SPOT ? 
        std::tan(fov * 0.5f) * farClip : 
        lightProjectionSize / 2;
    float halfNearLength = light.Type == LIGHT_TYPE_SPOT ?
        std::tan(fov * 0.5f) * nearClip :
        halfFarLength;

    DirectX::XMVECTOR fwd = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&light.Direction));
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // Up: World up vector (Y axis)
    DirectX::XMVECTOR right = DirectX::XMVector3Cross(up, fwd);

    DirectX::XMVECTOR frontMultFar = DirectX::XMVectorScale(fwd, farClip);
    DirectX::XMVECTOR rightMultFarLength = DirectX::XMVectorScale(right, halfFarLength);
    DirectX::XMVECTOR upMultFarLength = DirectX::XMVectorScale(up, halfFarLength);

    DirectX::XMVECTOR frontMultNear = DirectX::XMVectorScale(fwd, nearClip);
    DirectX::XMVECTOR rightMultNearLength = DirectX::XMVectorScale(right, halfNearLength);
    DirectX::XMVECTOR upMultNearLength = DirectX::XMVectorScale(up, halfNearLength);

    DirectX::XMVECTOR pos = light.Type == LIGHT_TYPE_SPOT ?
        DirectX::XMLoadFloat3(&light.Position) :
        DirectX::XMVectorScale(fwd, farClip * -0.5f);
    DirectX::XMVECTOR farCenter = DirectX::XMVectorAdd(frontMultFar, pos);
    DirectX::XMVECTOR nearCenter = DirectX::XMVectorAdd(frontMultFar, pos);

    // Points
    {
        DirectX::XMStoreFloat3(&frustum.points[0],		// Top Right Far
            DirectX::XMVectorAdd(farCenter,
                DirectX::XMVectorAdd(upMultFarLength, rightMultFarLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[1],		// Bottom Left Far
            DirectX::XMVectorSubtract(farCenter,
                DirectX::XMVectorAdd(upMultFarLength, rightMultFarLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[2],		// Top Left Far
            DirectX::XMVectorAdd(farCenter,
                DirectX::XMVectorSubtract(upMultFarLength, rightMultFarLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[3],		// Bottom Right Far
            DirectX::XMVectorSubtract(farCenter,
                DirectX::XMVectorSubtract(upMultFarLength, rightMultFarLength)
            ));


        DirectX::XMStoreFloat3(&frustum.points[4],		// Top Right Near
            DirectX::XMVectorAdd(nearCenter,
                DirectX::XMVectorAdd(upMultNearLength, rightMultNearLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[5],		// Bottom Left Near
            DirectX::XMVectorSubtract(nearCenter,
                DirectX::XMVectorAdd(upMultNearLength, rightMultNearLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[6],		// Top Left Near
            DirectX::XMVectorAdd(nearCenter,
                DirectX::XMVectorSubtract(upMultNearLength, rightMultNearLength)
            ));
        DirectX::XMStoreFloat3(&frustum.points[7],		// Bottom Right Near
            DirectX::XMVectorSubtract(nearCenter,
                DirectX::XMVectorSubtract(upMultNearLength, rightMultNearLength)
            ));
    }

    // Calculate Normals
    // https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
    {
        frustum.normals[0] = DirectX::XMFLOAT4(		// Near Face
            light.Direction.x,
            light.Direction.y,
            light.Direction.z,
            1.0
        );
        frustum.normals[0].w = CalcD(frustum.normals[0], frustum.points[4]);

        frustum.normals[1] = DirectX::XMFLOAT4(		// Far Face
            -light.Direction.x,
            -light.Direction.y,
            -light.Direction.z,
            1.0
        );
        frustum.normals[1].w = CalcD(frustum.normals[1], frustum.points[0]);

        DirectX::XMStoreFloat4(&frustum.normals[2],		// Left Face
            DirectX::XMVector3Cross(
                up,
                DirectX::XMVectorAdd(frontMultFar, rightMultFarLength)
            ));
        frustum.normals[2].w = CalcD(frustum.normals[2], frustum.points[1]);

        DirectX::XMStoreFloat4(&frustum.normals[3],		// Right Face
            DirectX::XMVector3Cross(
                DirectX::XMVectorSubtract(frontMultFar, rightMultFarLength),
                up
            ));
        frustum.normals[3].w = CalcD(frustum.normals[3], frustum.points[0]);

        DirectX::XMStoreFloat4(&frustum.normals[4],		// Bottom Face
            DirectX::XMVector3Cross(
                DirectX::XMVectorAdd(frontMultFar, upMultFarLength),
                right
            ));
        frustum.normals[4].w = CalcD(frustum.normals[4], frustum.points[1]);

        DirectX::XMStoreFloat4(&frustum.normals[5],		// Top Face
            DirectX::XMVector3Cross(
                right,
                DirectX::XMVectorSubtract(frontMultFar, upMultFarLength)
            ));
        frustum.normals[5].w = CalcD(frustum.normals[5], frustum.points[0]);
    }
}
