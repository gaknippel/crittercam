////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include "Camera.h"

namespace IGCS::GameSpecific::CameraManipulator
{
	// Has the camera routine run at least once since injection?
	bool isCameraFound();
	void displayCameraStructAddress();

	// Picks the render camera to override (from the destinations the camera routine was seen writing to), copies its current
	// state into our buffers and into 'camera'. Returns false if no camera is available yet.
	bool takeOverGameCamera(Camera& camera);
	// Lets the game write its own camera again.
	void releaseGameCamera();
	// Switch to the next camera the routine writes to, in case the first pick isn't the one you see on screen.
	void cycleCameraTarget();

	// Writes our free camera (manual movement) into the buffer the game copies from.
	void updateCameraDataInGameData(Camera& camera);
	// Writes an explicit pose (used by dolly playback).
	void writeCameraPose(const DirectX::XMFLOAT3& position, DirectX::FXMVECTOR rotationQuaternion, float fov);

	// Called when the client sends a changed setting. Nothing in the game needs updating for the current settings.
	void applySettingsToGameState();

	// Game speed / pause via the time dilation struct.
	void setTimeDilationPointerAddress(LPBYTE address);
	// Returns false if pausing isn't possible right now.
	bool setTimeStopValue(bool pauseGame);
	void reassertTimeStop();
}
