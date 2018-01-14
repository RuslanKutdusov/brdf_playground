#include "Precompiled.h"
#include "Camera.h"


void BaseCamera::SetPosition(const XMVECTOR& pos)
{
	SetOrientation(pos, m_dir, m_up);
}


void BaseCamera::SetDirection(const XMVECTOR& dir)
{
	SetOrientation(m_pos, dir, m_up);
}


void BaseCamera::SetUp(const XMVECTOR& up)
{
	SetOrientation(m_pos, m_dir, up);
}


void BaseCamera::SetOrientation(const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up)
{
	m_pos = pos;
	m_dir = dir;
	m_up = up;
}


void BaseCamera::SetNearFarPlanes(float nearZ, float farZ)
{
	m_nearZ = nearZ;
	m_farZ = farZ;
}


XMMATRIX BaseCamera::GetWorldToViewMatrix() const
{
	return XMMatrixLookToLH(m_pos, m_dir, m_up);
}


PerspectiveCamera::PerspectiveCamera()
{
	m_projectionType = kPerspectiveProjection;
}


void PerspectiveCamera::SetAspectRatio(float aspectRatio)
{
	m_aspectRatio = aspectRatio;
}


void PerspectiveCamera::SetFovY(float fovY)
{
	m_fovY = fovY;
}


XMMATRIX PerspectiveCamera::GetProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
}


void FirstPersonCamera::SetOrientation(const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up)
{
	BaseCamera::SetOrientation(pos, dir, up);

	if (XMVector3Equal(dir, XMVectorZero()) || XMVector3Equal(up, XMVectorZero()))
		return;

	XMFLOAT4X4 viewMatrix;
	XMStoreFloat4x4(&viewMatrix, GetWorldToViewMatrix());
	XMFLOAT3 zBasis = {
	    viewMatrix._13,
	    viewMatrix._23,
	    viewMatrix._33,
	};
	m_yawAngle = atan2f(zBasis.x, zBasis.z);
	float fLen = sqrtf(zBasis.z * zBasis.z + zBasis.x * zBasis.x);
	m_pitchAngle = -atan2f(zBasis.y, fLen);
}


void FirstPersonCamera::HandleInputEvent(const InputEvent& event)
{
	if (event.event == WM_RBUTTONDOWN)
	{
		m_prevMousePos.x = event.cursorX;
		m_prevMousePos.y = event.cursorY;
	}
	else if (event.event == WM_MOUSEWHEEL)
	{
		m_speed += event.mouseWheel / 120.0f;
		m_speed = std::max(m_speed, 0.01f);
	}
}


void FirstPersonCamera::Update(const InputState& inputState, float elapsedTime)
{
	XMFLOAT3 dir;
	memset(&dir, 0, sizeof(dir));
	if (inputState.keyPressed['W'])
		dir.z += 1.0f;
	if (inputState.keyPressed['S'])
		dir.z -= 1.0f;
	if (inputState.keyPressed['A'])
		dir.x -= 1.0f;
	if (inputState.keyPressed['D'])
		dir.x += 1.0f;
	if (inputState.keyPressed['E'])
		dir.y += 1.0f;
	if (inputState.keyPressed['Q'])
		dir.y -= 1.0f;

	XMVECTOR velocity = XMLoadFloat3(&dir);
	velocity = XMVector3Normalize(velocity);
	velocity = velocity * m_speed;

	if (inputState.keyPressed[VK_CONTROL])
		velocity = velocity * 2.0f;

	XMINT2 mousePosDelta = {0, 0};
	if (inputState.keyPressed[VK_RBUTTON])
	{
		mousePosDelta.x = inputState.cursorX - m_prevMousePos.x;
		mousePosDelta.y = inputState.cursorY - m_prevMousePos.y;
		m_prevMousePos = {inputState.cursorX, inputState.cursorY};

		m_yawAngle += mousePosDelta.x * m_rotationSpeed;
		m_pitchAngle += mousePosDelta.y * m_rotationSpeed;

		m_pitchAngle = std::max(-XM_PI / 2.0f, m_pitchAngle);
		m_pitchAngle = std::min(+XM_PI / 2.0f, m_pitchAngle);
	}

	XMMATRIX rotatiion = XMMatrixRotationRollPitchYaw(m_pitchAngle, m_yawAngle, 0);
	m_pos = m_pos + XMVector3TransformCoord(velocity * elapsedTime, rotatiion);
	m_dir = XMVector3TransformCoord(g_XMIdentityR2, rotatiion);
	m_up = XMVector3TransformCoord(g_XMIdentityR1, rotatiion);
}


void OrbitCamera::SetOrientation(const XMVECTOR& pos, const XMVECTOR& center, const XMVECTOR& up)
{
	m_center = center;

	XMVECTOR dir = XMVectorSubtract(center, pos);
	m_distance = XMVectorGetX(XMVector3Length(dir));

	dir = XMVector3Normalize(dir);
	BaseCamera::SetOrientation(pos, dir, up);

	dir = XMVectorScale(dir, -1.0f);
	float x = XMVectorGetX(dir);
	float y = XMVectorGetY(dir);
	float z = XMVectorGetZ(dir);
	m_yawAngle = atan2f(x, z);
	float fLen = sqrtf(z * z + x * x);
	m_pitchAngle = atan2f(y, fLen);
}


void OrbitCamera::HandleInputEvent(const InputEvent& event)
{
	if (event.event == WM_RBUTTONDOWN)
	{
		m_prevMousePos.x = event.cursorX;
		m_prevMousePos.y = event.cursorY;
	}
	else if (event.event == WM_MOUSEWHEEL)
	{
		m_distance -= event.mouseWheel / 120.0f;
		m_distance = std::max(m_distance, 1.0f);
	}
}


void OrbitCamera::Update(const InputState& inputState, float elapsedTime)
{
	XMINT2 mousePosDelta = {0, 0};
	if (inputState.keyPressed[VK_RBUTTON])
	{
		mousePosDelta.x = inputState.cursorX - m_prevMousePos.x;
		mousePosDelta.y = inputState.cursorY - m_prevMousePos.y;
		m_prevMousePos = {inputState.cursorX, inputState.cursorY};

		m_yawAngle += mousePosDelta.x * m_rotationSpeed;
		m_pitchAngle += mousePosDelta.y * m_rotationSpeed;

		m_pitchAngle = std::max(-XM_PI / 2.0f, m_pitchAngle);
		m_pitchAngle = std::min(+XM_PI / 2.0f, m_pitchAngle);
	}

	XMVECTOR dir = XMVectorSet(sin(m_yawAngle) * cos(m_pitchAngle), sin(m_pitchAngle), cos(m_yawAngle) * cos(m_pitchAngle), 0.0f);
	m_dir = XMVector3Normalize(XMVectorScale(dir, -1.0f));
	m_pos = XMVectorScale(dir, m_distance);
	m_pos = XMVectorAdd(m_pos, m_center);
}