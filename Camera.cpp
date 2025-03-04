#include "Camera.h"
#include "Input.h"

using namespace DirectX;


Camera::Camera(
	float x,
	float y,
	float z,
	float moveSpeed,
	float mouseLookSpeed,
	float fieldOfView,
	float aspectRatio,
	float nearClip,
	float farClip,
	CameraProjectionType projType)
	:
	movementSpeed(moveSpeed),
	mouseLookSpeed(mouseLookSpeed),
	fieldOfView(fieldOfView),
	aspectRatio(aspectRatio),
	nearClip(nearClip),
	farClip(farClip),
	projectionType(projType),
	orthographicWidth(2.0f),
	dirtyView(true)
{
	transform.SetPosition(x, y, z);

	UpdateViewMatrix();
	UpdateProjectionMatrix(aspectRatio);
}

Camera::Camera(
	DirectX::XMFLOAT3 position,
	float moveSpeed,
	float mouseLookSpeed,
	float fieldOfView,
	float aspectRatio,
	float nearClip,
	float farClip,
	CameraProjectionType projType)
	:
	movementSpeed(moveSpeed),
	mouseLookSpeed(mouseLookSpeed),
	fieldOfView(fieldOfView),
	aspectRatio(aspectRatio),
	nearClip(nearClip),
	farClip(farClip),
	projectionType(projType),
	dirtyView(true)
{
	transform.SetPosition(position);

	UpdateViewMatrix();
	UpdateProjectionMatrix(aspectRatio);
}

// Nothing to really do
Camera::~Camera()
{ }


// Camera's update, which looks for key presses
void Camera::Update(float dt)
{
	// Current speed
	float speed = dt * movementSpeed;// Speed up or down as necessary
	if (Input::KeyDown(VK_CONTROL)) { speed *= 5; }
	if (Input::KeyDown('X')) { speed *= 0.1f; }

	// Movement
	if (Input::KeyDown('W')) { transform.MoveRelative(0, 0, speed); dirtyView = true; }
	if (Input::KeyDown('S')) { transform.MoveRelative(0, 0, -speed); dirtyView = true; }
	if (Input::KeyDown('A')) { transform.MoveRelative(-speed, 0, 0); dirtyView = true; }
	if (Input::KeyDown('D')) { transform.MoveRelative(speed, 0, 0); dirtyView = true; }
	if (Input::KeyDown(VK_SHIFT)) { transform.MoveAbsolute(0, -speed, 0); dirtyView = true; }
	if (Input::KeyDown(' ')) { transform.MoveAbsolute(0, speed, 0); dirtyView = true; }

	// Handle mouse movement only when button is down
	if (Input::MouseLeftDown())
	{
		dirtyView = true;

		// Calculate cursor change
		float xDiff = mouseLookSpeed * Input::GetMouseXDelta();
		float yDiff = mouseLookSpeed * Input::GetMouseYDelta();
		transform.Rotate(yDiff, xDiff, 0);

		// Clamp the X rotation
		XMFLOAT3 rot = transform.GetPitchYawRoll();
		if (rot.x >= XM_PIDIV2) rot.x = XM_PIDIV2 - 0.00001f;
		if (rot.x <= -XM_PIDIV2) rot.x = -XM_PIDIV2 + 0.00001f;
		transform.SetRotation(rot);
	}

	// Update the view when changed
	if (dirtyView) 
	{ 
		UpdateViewMatrix(); 
		UpdateFrustum();
	}

}

// Creates a new view matrix based on current position and orientation
void Camera::UpdateViewMatrix()
{
	// Get the camera's forward vector and position
	XMFLOAT3 forward = transform.GetForward();
	XMFLOAT3 pos = transform.GetPosition();

	// Make the view matrix and save
	XMMATRIX view = XMMatrixLookToLH(
		XMLoadFloat3(&pos),
		XMLoadFloat3(&forward),
		XMVectorSet(0, 1, 0, 0)); // World up axis
	XMStoreFloat4x4(&viewMatrix, view);

	dirtyView = false;
}

// Updates the projection matrix
void Camera::UpdateProjectionMatrix(float aspectRatio)
{
	this->aspectRatio = aspectRatio;

	XMMATRIX P;

	// Which type?
	if (projectionType == CameraProjectionType::Perspective)
	{
		P = XMMatrixPerspectiveFovLH(
			fieldOfView,		// Field of View Angle
			aspectRatio,		// Aspect ratio
			nearClip,			// Near clip plane distance
			farClip);			// Far clip plane distance
	}
	else // CameraProjectionType::ORTHOGRAPHIC
	{
		P = XMMatrixOrthographicLH(
			orthographicWidth,	// Projection width (in world units)
			orthographicWidth / aspectRatio,// Projection height (in world units)
			nearClip,			// Near clip plane distance 
			farClip);			// Far clip plane distance
	}

	XMStoreFloat4x4(&projMatrix, P);
}

void Camera::UpdateFrustum()
{
	
	
	// Normals
	/*
	//https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
	frustum.normals[0] = transform.GetForward();		// Near Face
	DirectX::XMStoreFloat3(&frustum.normals[1],		// Far Face
		DirectX::XMVector3Normalize(
		DirectX::XMVectorScale(fwd, -1)));

	DirectX::XMStoreFloat3(&frustum.normals[2],		// Right Face
		DirectX::XMVector3Normalize(
		DirectX::XMVector3Cross(
			DirectX::XMVectorSubtract( frontMultFar, rightMultFarWidth),
			up
		)));
	DirectX::XMStoreFloat3(&frustum.normals[3],		// Left Face
		DirectX::XMVector3Normalize(
		DirectX::XMVector3Cross(
			up,
			DirectX::XMVectorAdd(frontMultFar, rightMultFarWidth)
		)));

	DirectX::XMStoreFloat3(&frustum.normals[4],		// Top Face
		DirectX::XMVector3Normalize(
		DirectX::XMVector3Cross(
			right,
			DirectX::XMVectorSubtract(frontMultFar, upMultFarHeight)
		)));
	DirectX::XMStoreFloat3(&frustum.normals[5],		// Bottom Face
		DirectX::XMVector3Normalize(
		DirectX::XMVector3Cross(
			DirectX::XMVectorAdd(frontMultFar, upMultFarHeight),
			right
		)));
		*/
	
	// https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
	DirectX::XMFLOAT4X4 viewTemp = GetView();
	DirectX::XMFLOAT4X4 projTemp = GetProjection();
	DirectX::XMFLOAT4X4 vp;
	DirectX::XMStoreFloat4x4(&vp,
		DirectX::XMMatrixMultiply(
			DirectX::XMLoadFloat4x4(&projTemp),
			DirectX::XMLoadFloat4x4(&viewTemp)
		));

	frustum.normals[0].x = vp._13;					// Near Face
	frustum.normals[0].y = vp._23;
	frustum.normals[0].z = vp._33;
	frustum.normals[0].w = vp._43;

	frustum.normals[1].x = vp._14 - vp._13;			// Far Face
	frustum.normals[1].y = vp._24 - vp._23;
	frustum.normals[1].z = vp._34 - vp._33;
	frustum.normals[1].w = vp._44 - vp._43;

	frustum.normals[2].x = vp._14 + vp._11;			// Left Face
	frustum.normals[2].y = vp._24 + vp._21;			
	frustum.normals[2].z = vp._34 + vp._31;
	frustum.normals[2].w = vp._44 + vp._41;

	frustum.normals[3].x = vp._14 - vp._11;			// Right Face
	frustum.normals[3].y = vp._24 - vp._21;
	frustum.normals[3].z = vp._34 - vp._31;
	frustum.normals[3].w = vp._44 - vp._41;

	frustum.normals[4].x = vp._14 + vp._12;			// Bottom Face
	frustum.normals[4].y = vp._24 + vp._22;
	frustum.normals[4].z = vp._34 + vp._32;
	frustum.normals[4].w = vp._44 + vp._42;

	frustum.normals[5].x = vp._14 - vp._12;			// Top Face
	frustum.normals[5].y = vp._24 - vp._22;
	frustum.normals[5].z = vp._34 - vp._32;
	frustum.normals[5].z = vp._44 - vp._42;

	// Variables
	const float halfFarHeight = std::tan(fieldOfView * 0.5f) * farClip;
	const float halfFarWidth = halfFarHeight * aspectRatio;
	const float halfNearHeight = std::tan(fieldOfView * 0.5f) * nearClip;
	const float halfNearWidth = halfNearHeight * aspectRatio;

	DirectX::XMFLOAT3 fwdTemp = transform.GetForward();
	DirectX::XMFLOAT3 upTemp = transform.GetUp();
	DirectX::XMFLOAT3 rightTemp = transform.GetRight();
	DirectX::XMVECTOR fwd = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&fwdTemp));
	DirectX::XMVECTOR up = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&upTemp));
	DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&rightTemp));

	DirectX::XMVECTOR frontMultFar = DirectX::XMVectorScale(fwd, farClip);
	DirectX::XMVECTOR rightMultFarWidth = DirectX::XMVectorScale(right, halfFarWidth);
	DirectX::XMVECTOR upMultFarHeight = DirectX::XMVectorScale(up, halfFarHeight);

	DirectX::XMVECTOR frontMultNear = DirectX::XMVectorScale(fwd, nearClip);
	DirectX::XMVECTOR rightMultNearWidth = DirectX::XMVectorScale(right, halfNearWidth);
	DirectX::XMVECTOR upMultNearHeight = DirectX::XMVectorScale(up, halfNearHeight);

	DirectX::XMFLOAT3 posTemp = transform.GetPosition();
	DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&posTemp);
	DirectX::XMVECTOR farCenter = DirectX::XMVectorAdd(frontMultFar, pos);
	DirectX::XMVECTOR nearCenter = DirectX::XMVectorAdd(frontMultNear, pos);

	// Points
	DirectX::XMStoreFloat3(&frustum.points[0],		// Top Right Far
		DirectX::XMVectorAdd(farCenter,
			DirectX::XMVectorAdd(upMultFarHeight, rightMultFarWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[1],		// Bottom Left Far
		DirectX::XMVectorSubtract(farCenter,
			DirectX::XMVectorAdd(upMultFarHeight, rightMultFarWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[2],		// Top Left Far
		DirectX::XMVectorAdd(farCenter,
			DirectX::XMVectorSubtract(upMultFarHeight, rightMultFarWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[3],		// Bottom Right Far
		DirectX::XMVectorSubtract(farCenter,
			DirectX::XMVectorSubtract(upMultFarHeight, rightMultFarWidth)
		));


	DirectX::XMStoreFloat3(&frustum.points[4],		// Top Right Near
		DirectX::XMVectorAdd(nearCenter,
			DirectX::XMVectorAdd(upMultNearHeight, rightMultNearWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[5],		// Bottom Left Near
		DirectX::XMVectorSubtract(nearCenter,
			DirectX::XMVectorAdd(upMultNearHeight, rightMultNearWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[6],		// Top Left Near
		DirectX::XMVectorAdd(nearCenter,
			DirectX::XMVectorSubtract(upMultNearHeight, rightMultNearWidth)
		));
	DirectX::XMStoreFloat3(&frustum.points[7],		// Bottom Right Near
		DirectX::XMVectorSubtract(nearCenter,
			DirectX::XMVectorSubtract(upMultNearHeight, rightMultNearWidth)
		));
}

DirectX::XMFLOAT4X4 Camera::GetView()
{
	if (dirtyView) { UpdateViewMatrix(); }
	return viewMatrix;
}
DirectX::XMFLOAT4X4 Camera::GetProjection() { return projMatrix; }
Transform* Camera::GetTransform()
{
	dirtyView = true; // Assume some change is being made
	return &transform;
}

float Camera::GetAspectRatio() { return aspectRatio; }

float Camera::GetFieldOfView() { return fieldOfView; }
void Camera::SetFieldOfView(float fov)
{
	fieldOfView = fov;
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetMovementSpeed() { return movementSpeed; }
void Camera::SetMovementSpeed(float speed)
{
	if (speed < 0.1f) { movementSpeed = 0.1f; return; }
	movementSpeed = speed;
}

float Camera::GetMouseLookSpeed() { return mouseLookSpeed; }
void Camera::SetMouseLookSpeed(float speed)
{
	if (speed < 0.001f) { mouseLookSpeed = 0.001f; return; }
	if (speed > 0.1f) { mouseLookSpeed = 0.1f; return; }
	mouseLookSpeed = speed;
}

float Camera::GetNearClip() { return nearClip; }
void Camera::SetNearClip(float distance)
{
	if (distance < 0.005f) { nearClip = 0.005f; }
	else if (distance >= farClip) { nearClip = farClip - 1; }
	else { nearClip = distance; }
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetFarClip() { return farClip; }
void Camera::SetFarClip(float distance)
{
	if (distance <= nearClip) { farClip = nearClip + 1; return; }
	else if (distance > 1500.0f) { farClip = 1500.0f; return; }
	else { farClip = distance; }
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetOrthographicWidth() { return orthographicWidth; }
void Camera::SetOrthographicWidth(float width)
{
	orthographicWidth = width;
	UpdateProjectionMatrix(aspectRatio);
}

CameraProjectionType Camera::GetProjectionType() { return projectionType; }
void Camera::SetProjectionType(CameraProjectionType type)
{
	projectionType = type;
	UpdateProjectionMatrix(aspectRatio);
}

Frustum Camera::GetFrustum() 
{ 
	if (dirtyView)
	{
		UpdateViewMatrix();
		UpdateFrustum();
	}
	return frustum; 
}

/*
std::shared_ptr<Camera> Camera::Parse(nlohmann::json jsonCamera)
{
	// Defaults
	CameraProjectionType projType = CameraProjectionType::Perspective;
	float moveSpeed = 5.0f;
	float lookSpeed = 0.002f;
	float fov = XM_PIDIV4;
	float nearClip = 0.01f;
	float farClip = 1000.0f;
	XMFLOAT3 pos = { 0, 0, -5 };
	XMFLOAT3 rot = { 0, 0, 0 };

	// Check for each type of data
	if (jsonCamera.contains("type") && jsonCamera["type"].get<std::string>() == "orthographic")
		projType = CameraProjectionType::Orthographic;

	if (jsonCamera.contains("moveSpeed")) moveSpeed = jsonCamera["moveSpeed"].get<float>();
	if (jsonCamera.contains("lookSpeed")) lookSpeed = jsonCamera["lookSpeed"].get<float>();
	if (jsonCamera.contains("fov")) fov = jsonCamera["fov"].get<float>();
	if (jsonCamera.contains("near")) nearClip = jsonCamera["near"].get<float>();
	if (jsonCamera.contains("far")) farClip = jsonCamera["far"].get<float>();
	if (jsonCamera.contains("position") && jsonCamera["position"].size() == 3)
	{
		pos.x = jsonCamera["position"][0].get<float>();
		pos.y = jsonCamera["position"][1].get<float>();
		pos.z = jsonCamera["position"][2].get<float>();
	}
	if (jsonCamera.contains("rotation") && jsonCamera["rotation"].size() == 3)
	{
		rot.x = jsonCamera["rotation"][0].get<float>();
		rot.y = jsonCamera["rotation"][1].get<float>();
		rot.z = jsonCamera["rotation"][2].get<float>();
	}

	// Create the camera
	std::shared_ptr<Camera> cam = std::make_shared<Camera>(
		pos, moveSpeed, lookSpeed, fov, 1.0f, nearClip, farClip, projType);
	cam->GetTransform()->SetRotation(rot);

	return cam;
}
*/


