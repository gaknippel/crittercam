////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "Camera.h"
#include "Defaults.h"
#include "GameConstants.h"
#include "Globals.h"
#include <algorithm>

using namespace DirectX;

namespace IGCS
{
	Camera::Camera() : _direction(XMFLOAT3(0.0f, 0.0f, 0.0f)), _position(XMFLOAT3(0.0f, 0.0f, 0.0f)), _yaw(0), _pitch(0), _roll(0),
					   _fov(DEFAULT_FOV), _originalFov(DEFAULT_FOV), _movementOccurred(false)
	{
	}


	Camera::~Camera(void)
	{
	}


	void Camera::initializeFromGameMatrix(const float* m, float fov)
	{
		// m is a 4x4 row-major matrix: m[0..3] right, m[4..7] up, m[8..11] forward, m[12..15] position.
		// For a roll/pitch/yaw matrix: forward = (sin(yaw)cos(pitch), -sin(pitch), cos(yaw)cos(pitch)),
		// right.y = sin(roll)cos(pitch), up.y = cos(roll)cos(pitch).
		const float forwardX = m[8], forwardY = m[9], forwardZ = m[10];
		_pitch = clampAngle(asinf(std::clamp(-forwardY, -1.0f, 1.0f)));
		_yaw = clampAngle(atan2f(forwardX, forwardZ));
		_roll = clampAngle(atan2f(m[1], m[5]));
		_position = XMFLOAT3(m[12], m[13], m[14]);
		_fov = fov;
		_originalFov = fov;
		resetMovement();
	}


	XMMATRIX Camera::calculateRotationMatrix() const
	{
		return XMMatrixRotationRollPitchYaw(_pitch, _yaw, _roll);
	}


	XMVECTOR Camera::calculateRotationQuaternion() const
	{
		return XMQuaternionNormalize(XMQuaternionRotationRollPitchYaw(_pitch, _yaw, _roll));
	}


	void Camera::applyMovement()
	{
		if (!_movementOccurred)
		{
			return;
		}
		XMMATRIX rotation = calculateRotationMatrix();
		XMVECTOR right = rotation.r[0];
		XMVECTOR forward = rotation.r[2];
		XMVECTOR position = XMLoadFloat3(&_position);
		position = XMVectorAdd(position, XMVectorScale(right, _direction.x));
		position = XMVectorAdd(position, XMVectorScale(forward, _direction.z));
		position = XMVectorAdd(position, XMVectorSet(0.0f, _direction.y, 0.0f, 0.0f));	// up/down is always world up, nicer for dolly shots
		XMStoreFloat3(&_position, position);
	}


	void Camera::resetMovement()
	{
		_movementOccurred = false;
		_direction.x = 0.0f;
		_direction.y = 0.0f;
		_direction.z = 0.0f;
	}


	void Camera::resetAngles()
	{
		setPitch(INITIAL_PITCH_RADIANS);
		setRoll(INITIAL_ROLL_RADIANS);
		setYaw(INITIAL_YAW_RADIANS);
	}


	void Camera::moveForward(float amount)
	{
		_direction.z += (Globals::instance().settings().movementSpeed * amount);		// +z is forward
		_movementOccurred = true;
	}


	void Camera::moveRight(float amount)
	{
		_direction.x += (Globals::instance().settings().movementSpeed * amount);		// +x is right
		_movementOccurred = true;
	}


	void Camera::moveUp(float amount)
	{
		_direction.y += (Globals::instance().settings().movementSpeed * amount * Globals::instance().settings().movementUpMultiplier);  // +y is up
		_movementOccurred = true;
	}


	void Camera::yaw(float amount)
	{
		// positive yaw turns right
		_yaw += (Globals::instance().settings().rotationSpeed * amount);
		_yaw = clampAngle(_yaw);
	}


	void Camera::pitch(float amount)
	{
		// positive amount = look up. A positive pitch angle looks down in this convention, hence the minus.
		float lookDirectionInverter = Globals::instance().settings().invertY ? -1.0f : 1.0f;
		// stop just short of straight up/down like a game camera, instead of flipping over the top.
		float signedPitch = _pitch > XM_PI ? _pitch - XM_2PI : _pitch;
		signedPitch -= (Globals::instance().settings().rotationSpeed * amount * lookDirectionInverter);
		_pitch = clampAngle(std::clamp(signedPitch, -PITCH_LIMIT, PITCH_LIMIT));
	}


	void Camera::roll(float amount)
	{
		// positive roll = tilt left
		_roll += (Globals::instance().settings().rotationSpeed * amount);
		_roll = clampAngle(_roll);
	}


	void Camera::changeFoV(float amount)
	{
		_fov = std::clamp(_fov + amount, FOV_MIN, FOV_MAX);
	}


	void Camera::setPitch(float angle)
	{
		_pitch = clampAngle(angle);
	}


	void Camera::setYaw(float angle)
	{
		_yaw = clampAngle(angle);
	}


	void Camera::setRoll(float angle)
	{
		_roll = clampAngle(angle);
	}


	// Keep the angle in the range 0 to 360 (2*PI)
	float Camera::clampAngle(float angle) const
	{
		while (angle > XM_2PI)
		{
			angle -= XM_2PI;
		}
		while (angle < 0)
		{
			angle += XM_2PI;
		}
		return angle;
	}
}
