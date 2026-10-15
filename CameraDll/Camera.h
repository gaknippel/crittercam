////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"

namespace IGCS
{
	// Our free camera. Conventions (DirectX, left handed, same as the game): +X right, +Y up, +Z forward.
	// Rotation = roll (Z), then pitch (X), then yaw (Y), which is what XMMatrixRotationRollPitchYaw builds.
	// The rows of the rotation matrix are the camera's right, up and forward vectors in world space.
	class Camera
	{
	public:
		Camera();
		~Camera(void);

		// Takes over the game camera: angles from the matrix rows, position from row 3, the fov.
		void initializeFromGameMatrix(const float* matrix4x4, float fov);
		DirectX::XMMATRIX calculateRotationMatrix() const;
		DirectX::XMVECTOR calculateRotationQuaternion() const;
		// Applies the movement collected this frame to the position, relative to the current rotation.
		void applyMovement();
		void resetMovement();
		void resetAngles();
		void moveForward(float amount);
		void moveRight(float amount);
		void moveUp(float amount);
		void yaw(float amount);
		void pitch(float amount);
		void roll(float amount);
		void setPitch(float angle);
		void setYaw(float angle);
		void setRoll(float angle);
		float getPitch() const { return _pitch; }
		float getYaw() const { return _yaw; }
		float getRoll() const { return _roll; }
		DirectX::XMFLOAT3 getPosition() const { return _position; }
		void setPosition(DirectX::XMFLOAT3 newPosition) { _position = newPosition; }
		float getFoV() const { return _fov; }
		void changeFoV(float amount);
		void resetFoV() { _fov = _originalFov; }
		float getOriginalFoV() const { return _originalFov; }

	private:
		float clampAngle(float angle) const;

		DirectX::XMFLOAT3 _direction;		// movement collected this frame, in camera space (x right, y world up, z forward)
		DirectX::XMFLOAT3 _position;
		float _yaw;
		float _pitch;
		float _roll;
		float _fov;
		float _originalFov;
		bool _movementOccurred;
	};
}
