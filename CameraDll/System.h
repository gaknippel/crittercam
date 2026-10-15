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
		void toggleCameraMovementLockState();
		void handleKeyboardCameraMovement(float multiplier);
		void handleMouseCameraMovement(float multiplier);
		void handleGamePadMovement(float multiplierBase);
		void waitForCameraStructAddresses();
		void toggleInputBlockState();
		void toggleHud();
		void toggleGamePause(bool displayNotification = true);
		void handleSkipFrames();
		void applyHammerPrevention();

		Camera _camera;
		CameraPath _cameraPath;
		LPBYTE _hostImageAddress;
		DWORD _hostImageSize;
		bool _cameraStructFound = false;
		map<string, AOBBlock*> _aobBlocks;
		ULONGLONG _ignoreKeysUntil = 0;		// key repeat prevention without blocking the loop (a blocking sleep would stutter dolly playback)
		std::filesystem::path _hostExePath;
		std::filesystem::path _hostExeFilename;
	};
}
