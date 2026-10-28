////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include "Camera.h"
#include "CameraPath.h"
#include "InputHooker.h"
#include "Gamepad.h"
#include <map>
#include <chrono>
#include "AOBBlock.h"

namespace IGCS
{
	class System
	{
	public:
		System();
		~System();
		void start(LPBYTE hostBaseAddress, DWORD hostImageSize);

	private:
		void mainLoop();
		void initialize();
		void updateFrame();
		bool checkIfGamehasFocus();
		void handleUserInput();
		void handleDollyInput();
		void displayCameraState();
		void toggleCamera();
		void followWorldShift();
		void updateTracking(bool follow);
		void toggleCameraMovementLockState();
		void handleKeyboardCameraMovement(float multiplier);
		void handleMouseCameraMovement(float multiplier);
		void handleGamePadMovement(float multiplierBase, float frameScale);
		void waitForCameraStructAddresses();
		void toggleInputBlockState();
		void toggleHud();
		void toggleGamePause(bool displayNotification = true);
		void handleSkipFrames();
		void applyHammerPrevention();
		void sendCameraState();
		void updateGameSpeed();
		void handleGameSpeedWheel();

		Camera _camera;
		CameraPath _cameraPath;
		LPBYTE _hostImageAddress;
		DWORD _hostImageSize;
		bool _cameraStructFound = false;
		map<string, AOBBlock*> _aobBlocks;
		std::vector<DirectX::XMFLOAT3> _pathCurve, _pathNodes, _pathAims;	// reused buffers for the path overlay
		std::chrono::steady_clock::time_point _lastInputTime = std::chrono::steady_clock::now();
		LPBYTE _worldChrManPointer = nullptr;		// address of the game's global WorldChrMan pointer, nullptr if the pattern wasn't found
		ULONGLONG _nextStateSend = 0;
		float _appliedGameSpeed = 1.0f;		// what we last wrote into the game, eases toward the setting
		std::chrono::steady_clock::time_point _lastSpeedUpdate = std::chrono::steady_clock::now();
		// tracking camera, see updateTracking
		bool _trackActive = false;
		DirectX::XMFLOAT3 _trackAnchor = {};		// the lagging point the camera looks at, chases the player's chest
		DirectX::XMFLOAT3 _trackOffset = {};		// camera position - anchor, kept while following
		float _trackYawOffset = 0.0f, _trackPitchOffset = 0.0f;		// framing: how far the shot is turned away from the anchor
		float _trackLastYaw = 0.0f, _trackLastPitch = 0.0f;
		std::chrono::steady_clock::time_point _trackLastUpdate;
		ULONGLONG _ignoreKeysUntil = 0;		// key repeat prevention without blocking the loop (a blocking sleep would stutter dolly playback)
		std::filesystem::path _hostExePath;
		std::filesystem::path _hostExeFilename;
	};
}
