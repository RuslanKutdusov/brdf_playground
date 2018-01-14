#pragma once
#include "Input.h"

enum EProjectionType
{
	kInvalidProjection = 0,
	kOrthoProjection,
	kPerspectiveProjection
};


class BaseCamera
{
public:
	BaseCamera() = default;
	~BaseCamera() = default;

	EProjectionType GetProjectionType() const;

	virtual void SetPosition(const XMVECTOR& pos);
	virtual void SetDirection(const XMVECTOR& dir);
	virtual void SetUp(const XMVECTOR& up);
	virtual void SetOrientation(const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up);
	virtual void SetNearFarPlanes(float nearZ, float farZ);

	const XMVECTOR& GetPosition() const;
	const XMVECTOR& GetDirection() const;
	const XMVECTOR& GetUp() const;
	float GetNearZ() const;
	float GetFarZ() const;

	XMMATRIX GetWorldToViewMatrix() const;
	virtual XMMATRIX GetProjectionMatrix() const = 0;

protected:
	EProjectionType m_projectionType = kInvalidProjection;
	XMVECTOR m_pos;
	XMVECTOR m_up;
	XMVECTOR m_dir;
	float m_nearZ;
	float m_farZ;
};


class PerspectiveCamera : public BaseCamera
{
public:
	PerspectiveCamera();

	void SetAspectRatio(float aspectRatio);
	void SetFovY(float fovY);

	float GetAspectRatio() const;
	float GetFovY() const;

	XMMATRIX GetProjectionMatrix() const;

	virtual void HandleInputEvent(const InputEvent& event) = 0;
	virtual void Update(const InputState& inputState, float elapsedTime) = 0;

protected:
	float m_aspectRatio;
	float m_fovY;
};


class FirstPersonCamera : public PerspectiveCamera
{
public:
	void SetOrientation(const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up);

	void HandleInputEvent(const InputEvent& event);
	void Update(const InputState& inputState, float elapsedTime);

protected:
	float m_speed = 4.0f;
	float m_rotationSpeed = 0.003f;
	XMINT2 m_prevMousePos = {0, 0};
	float m_yawAngle;
	float m_pitchAngle;
};


class OrbitCamera : public PerspectiveCamera
{
public:
	void SetOrientation(const XMVECTOR& pos, const XMVECTOR& center, const XMVECTOR& up);

	void HandleInputEvent(const InputEvent& event);
	void Update(const InputState& inputState, float elapsedTime);

protected:
	float m_rotationSpeed = 0.003f;
	XMINT2 m_prevMousePos = {0, 0};
	XMVECTOR m_center;
	float m_distance;
	float m_yawAngle;
	float m_pitchAngle;
};


inline EProjectionType BaseCamera::GetProjectionType() const
{
	return m_projectionType;
}


inline const XMVECTOR& BaseCamera::GetPosition() const
{
	return m_pos;
}


inline const XMVECTOR& BaseCamera::GetDirection() const
{
	return m_dir;
}


inline const XMVECTOR& BaseCamera::GetUp() const
{
	return m_up;
}


inline float BaseCamera::GetNearZ() const
{
	return m_nearZ;
}


inline float BaseCamera::GetFarZ() const
{
	return m_farZ;
}


inline float PerspectiveCamera::GetAspectRatio() const
{
	return m_aspectRatio;
}


inline float PerspectiveCamera::GetFovY() const
{
	return m_fovY;
}