////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "System.h"
#include "Globals.h"
#include "Defaults.h"
#include "GameConstants.h"
#include "Gamepad.h"
#include "CameraManipulator.h"
#include "InterceptorHelper.h"
#include "InputHooker.h"
#include "input.h"
#include "MinHook.h"
#include "NamedPipeManager.h"
#include "MessageHandler.h"
#include <timeapi.h>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

using namespace DirectX;

namespace IGCS
{
	using namespace IGCS::GameSpecific;

	System::System()
	{
	}


	System::~System()
	{
	}


	void System::start(LPBYTE hostBaseAddress, DWORD hostImageSize)
	{
		Globals::instance().systemActive(true);
		_hostImageAddress = (LPBYTE)hostBaseAddress;
		_hostImageSize = hostImageSize;
		filesystem::path hostExeFilenameAndPath = Utils::obtainHostExeAndPath();
		_hostExeFilename = hostExeFilenameAndPath.stem();
		_hostExePath = hostExeFilenameAndPath.parent_path();
		Globals::instance().gamePad().setInvertLStickY(CONTROLLER_Y_INVERT);
		Globals::instance().gamePad().setInvertRStickY(CONTROLLER_Y_INVERT);
		timeBeginPeriod(1);		// so Sleep(1) really sleeps ~1ms during dolly playback
		initialize();		// will block till camera is found
		mainLoop();
		timeEndPeriod(1);
	}


	// Core loop of the system
	void System::mainLoop()
	{
		while (Globals::instance().systemActive())
		{
			Sleep(_cameraPath.isPlaying() ? FRAME_SLEEP_DOLLY : FRAME_SLEEP);
			updateFrame();
		}
	}


	// updates the data and camera for a frame
	void System::updateFrame()
	{
		handleUserInput();
		if (g_cameraEnabled && _cameraPath.isPlaying())
		{
			XMFLOAT3 position;
			XMVECTOR rotation;
			float fov;
			bool stillPlaying = _cameraPath.evaluate(position, rotation, fov);
			CameraManipulator::writeCameraPose(position, rotation, fov);
			if (!stillPlaying)
			{
				MessageHandler::addNotification("Dolly playback finished");
			}
		}
		else
		{
			CameraManipulator::updateCameraDataInGameData(_camera);
		}
		if (Globals::instance().gamePaused())
		{
			CameraManipulator::reassertTimeStop();
		}
	}


	bool System::checkIfGamehasFocus()
	{
		HWND currentForegroundWindow = GetForegroundWindow();
		return (currentForegroundWindow == Globals::instance().mainWindowHandle());
	}


	void System::applyHammerPrevention()
	{
		_ignoreKeysUntil = GetTickCount64() + 200;
	}


	void System::handleUserInput()
	{
		if(!checkIfGamehasFocus())
		{
			// our window isn't focused, exit
			return;
		}

		Globals::instance().gamePad().update();
		if (GetTickCount64() < _ignoreKeysUntil)
		{
			// a toggle key was just pressed: ignore input for a bit, so holding the key doesn't toggle it again and again.
			return;
		}

		if (!_cameraStructFound)
		{
			// camera not found yet, can't proceed.
			return;
		}
		if (Input::isActionActivated(ActionType::CameraEnable))
		{
			toggleCamera();
			applyHammerPrevention();
			return;
		}
		if(Input::isActionActivated(ActionType::Timestop))
		{
			toggleGamePause();
			applyHammerPrevention();
		}
		if (Input::isActionActivated(ActionType::HudToggle))
		{
			toggleHud();
			applyHammerPrevention();
		}
		if (Input::isActionActivated(ActionType::SkipFrames))
		{
			handleSkipFrames();
			applyHammerPrevention();
		}

		if (!g_cameraEnabled)
		{
			// camera is disabled. We simply disable all input to the camera movement, by returning now.
			return;
		}
		if (Input::isActionActivated(ActionType::BlockInput))
		{
			toggleInputBlockState();
			applyHammerPrevention();
		}
		if (Input::isActionActivated(ActionType::CycleCameraTarget))
		{
			CameraManipulator::cycleCameraTarget();
			applyHammerPrevention();
		}
		handleDollyInput();
		if (_cameraPath.isPlaying())
		{
			// the dolly drives the camera, ignore manual movement.
			Input::resetMouseDeltas();
			return;
		}

		if (Input::isActionActivated(ActionType::FovReset) && Globals::instance().keyboardMouseControlCamera())
		{
			_camera.resetFoV();
		}
		if (Input::isActionActivated(ActionType::FovDecrease) && Globals::instance().keyboardMouseControlCamera())
		{
			_camera.changeFoV(-Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE);
		}
		if (Input::isActionActivated(ActionType::FovIncrease) && Globals::instance().keyboardMouseControlCamera())
		{
			_camera.changeFoV(Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE);
		}
		_camera.resetMovement();
		Settings& settings = Globals::instance().settings();
		if (Input::isActionActivated(ActionType::CameraLock))
		{
			toggleCameraMovementLockState();
			applyHammerPrevention();
		}
		if (Globals::instance().cameraMovementLocked())
		{
			// no movement allowed, simply return
			return;
		}

		float multiplier = Utils::altPressed() ? settings.fastMovementMultiplier : Utils::ctrlPressed() ? settings.slowMovementMultiplier : 1.0f;
		// Zoomed in = move slower, so it stays controllable. Relative to the fov the game had when the camera was enabled.
		multiplier *= std::clamp(fabsf(_camera.getFoV()) / _camera.getOriginalFoV(), 0.01f, 1.0f);
		handleKeyboardCameraMovement(multiplier);
		handleMouseCameraMovement(multiplier);
		handleGamePadMovement(multiplier);
	}


	void System::handleDollyInput()
	{
		if (Input::isActionActivated(ActionType::DollyPlayStop))
		{
			if (_cameraPath.isPlaying())
			{
				_cameraPath.stopPlayback();
				MessageHandler::addNotification("Dolly playback stopped");
			}
			else if (_cameraPath.startPlayback())
			{
				MessageHandler::addNotification(Utils::formatString("Dolly playback started (%d nodes)", _cameraPath.nodeCount()));
			}
			else
			{
				MessageHandler::addNotification("Add at least 2 nodes to play a dolly path");
			}
			applyHammerPrevention();
			return;
		}
		if (_cameraPath.isPlaying())
		{
			// editing the path while it plays isn't supported.
			return;
		}
		if (Input::isActionActivated(ActionType::DollyAddNode))
		{
			_cameraPath.addNode(_camera.getPosition(), _camera.calculateRotationQuaternion(), _camera.getFoV());
			MessageHandler::addNotification(Utils::formatString("Dolly node %d added", _cameraPath.nodeCount()));
			applyHammerPrevention();
		}
		if (Input::isActionActivated(ActionType::DollyDeleteLastNode))
		{
			if (_cameraPath.deleteLastNode())
			{
				MessageHandler::addNotification(Utils::formatString("Last dolly node deleted, %d left", _cameraPath.nodeCount()));
			}
			applyHammerPrevention();
		}
		if (Input::isActionActivated(ActionType::DollyClearPath))
		{
			_cameraPath.clear();
			MessageHandler::addNotification("Dolly path cleared");
			applyHammerPrevention();
		}
	}


	void System::toggleCamera()
	{
		if (g_cameraEnabled)
		{
			// it's going to be disabled: the game takes its camera back.
			_cameraPath.stopPlayback();
			g_cameraEnabled = 0;
			CameraManipulator::releaseGameCamera();
			Globals::instance().cameraMovementLocked(false);
		}
		else
		{
			// it's going to be enabled: start from where the game camera is right now.
			if (!CameraManipulator::takeOverGameCamera(_camera))
			{
				return;
			}
			g_cameraEnabled = 1;
		}
		displayCameraState();
	}


	void System::handleGamePadMovement(float multiplierBase)
	{
		if(!Globals::instance().controllerControlsCamera())
		{
			return;
		}

		auto gamePad = Globals::instance().gamePad();

		if (gamePad.isConnected())
		{
			Settings& settings = Globals::instance().settings();
			float  multiplier = gamePad.isButtonPressed(IGCS_BUTTON_FASTER) ? settings.fastMovementMultiplier
																			: gamePad.isButtonPressed(IGCS_BUTTON_SLOWER) ? settings.slowMovementMultiplier : multiplierBase;
			vec2 rightStickPosition = gamePad.getRStickPosition();
			_camera.pitch(rightStickPosition.y * multiplier);
			_camera.yaw(rightStickPosition.x * multiplier);

			vec2 leftStickPosition = gamePad.getLStickPosition();
			_camera.moveUp((gamePad.getLTrigger() - gamePad.getRTrigger()) * multiplier);
			_camera.moveForward(leftStickPosition.y * multiplier);
			_camera.moveRight(leftStickPosition.x * multiplier);

			if (gamePad.isButtonPressed(IGCS_BUTTON_TILT_LEFT))
			{
				_camera.roll(multiplier);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_TILT_RIGHT))
			{
				_camera.roll(-multiplier);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_RESET_FOV))
			{
				_camera.resetFoV();
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_FOV_DECREASE))
			{
				_camera.changeFoV(-Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_FOV_INCREASE))
			{
				_camera.changeFoV(Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE);
			}
		}
	}


	void System::handleMouseCameraMovement(float multiplier)
	{
		if (!Globals::instance().keyboardMouseControlCamera())
		{
			return;
		}
		long mouseDeltaX = Input::getMouseDeltaX();
		long mouseDeltaY = Input::getMouseDeltaY();
		bool leftButtonPressed = Input::isMouseButtonDown(0);
		bool rightButtonPressed = Input::isMouseButtonDown(1);
		bool noButtonPressed = !(leftButtonPressed || rightButtonPressed);
		if (abs(mouseDeltaY) > 1)
		{
			float yValue = (static_cast<float>(mouseDeltaY)* MOUSE_SPEED_CORRECTION* multiplier);
			if (noButtonPressed)
			{
				_camera.pitch(-yValue);
			}
			else
			{
				if(leftButtonPressed)
				{
					// move up / down
					_camera.moveUp(-yValue);
				}
				else
				{
					// forward/backwards
					_camera.moveForward(-yValue);
				}
			}
		}
		if (abs(mouseDeltaX) > 1)
		{
			float xValue = static_cast<float>(mouseDeltaX)* MOUSE_SPEED_CORRECTION* multiplier;
			if (noButtonPressed)
			{
				_camera.yaw(xValue);
			}
			else
			{
				// if both buttons are pressed: do tilt
				if(leftButtonPressed&&rightButtonPressed)
				{
					_camera.roll(-xValue);
				}
				else
				{
					// always left/right
					_camera.moveRight(xValue);
				}
			}
		}
		short mouseWheelDelta = Input::getMouseWheelDelta();
		if(abs(mouseWheelDelta) > 0)
		{
			_camera.changeFoV(-(static_cast<float>(mouseWheelDelta) * Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE));
		}
		Input::resetMouseDeltas();
	}


	void System::handleKeyboardCameraMovement(float multiplier)
	{
		if (!Globals::instance().keyboardMouseControlCamera())
		{
			return;
		}
		if (Input::isActionActivated(ActionType::MoveForward, true))
		{
			_camera.moveForward(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveBackward, true))
		{
			_camera.moveForward(-multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveRight, true))
		{
			_camera.moveRight(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveLeft, true))
		{
			_camera.moveRight(-multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveUp, true))
		{
			_camera.moveUp(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveDown, true))
		{
			_camera.moveUp(-multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateDown, true))
		{
			_camera.pitch(-multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateUp, true))
		{
			_camera.pitch(multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateRight, true))
		{
			_camera.yaw(multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateLeft, true))
		{
			_camera.yaw(-multiplier);
		}
		if (Input::isActionActivated(ActionType::TiltLeft, true))
		{
			_camera.roll(multiplier);
		}
		if (Input::isActionActivated(ActionType::TiltRight, true))
		{
			_camera.roll(-multiplier);
		}
	}


	// Initializes system. Will block till camera struct is found.
	void System::initialize()
	{
		MH_Initialize();
		// first grab the window handle
		Globals::instance().mainWindowHandle(Utils::findMainWindow(GetCurrentProcessId()));
		NamedPipeManager::instance().connectDllToClient();
		NamedPipeManager::instance().startListening();
		InputHooker::setInputHooks();
		Input::registerRawInput();

		GameSpecific::InterceptorHelper::initializeAOBBlocks(_hostImageAddress, _hostImageSize, _aobBlocks);
		GameSpecific::InterceptorHelper::setCameraStructInterceptorHook(_aobBlocks);
		waitForCameraStructAddresses();		// blocks till camera is found.
		GameSpecific::InterceptorHelper::setPostCameraStructHooks(_aobBlocks);

		// camera struct found, init our own camera object now and hook into game code which uses camera.
		_cameraStructFound = true;
		_camera.setPitch(INITIAL_PITCH_RADIANS);
		_camera.setRoll(INITIAL_ROLL_RADIANS);
		_camera.setYaw(INITIAL_YAW_RADIANS);
	}


	// Waits for the interceptor to pick up the camera struct address. Should only return if address is found
	void System::waitForCameraStructAddresses()
	{
		MessageHandler::logLine("Waiting for camera struct interception...");
		while(!GameSpecific::CameraManipulator::isCameraFound())
		{
			handleUserInput();
			Sleep(100);
		}
		MessageHandler::addNotification("Camera found.");
		GameSpecific::CameraManipulator::displayCameraStructAddress();
	}


	void System::toggleCameraMovementLockState()
	{
		MessageHandler::addNotification(Globals::instance().toggleCameraMovementLocked() ? "Camera movement is locked" : "Camera movement is unlocked");
	}


	void System::toggleInputBlockState()
	{
		MessageHandler::addNotification(Globals::instance().toggleInputBlocked() ? "Input to game blocked" : "Input to game enabled");
	}


	void System::toggleHud()
	{
		bool hudVisible = Globals::instance().toggleHudVisible();
		InterceptorHelper::toggleHud(_aobBlocks, hudVisible);
		MessageHandler::addNotification(hudVisible ? "HUD visible" : "HUD hidden");
	}


	void System::handleSkipFrames()
	{
		if(!Globals::instance().gamePaused())
		{
			// game not paused, don't skip frames
			return;
		}
		// unpause for ~2 frames, then pause again
		toggleGamePause(false);
		Sleep(32);
		toggleGamePause(false);
	}


	void System::toggleGamePause(bool displayNotification)
	{
		bool gamePaused = Globals::instance().toggleGamePaused();
		if (!CameraManipulator::setTimeStopValue(gamePaused))
		{
			Globals::instance().toggleGamePaused();		// didn't work, flip our state back
			return;
		}
		if (displayNotification)
		{
			MessageHandler::addNotification(gamePaused ? "Game paused" : "Game unpaused");
		}
	}


	void System::displayCameraState()
	{
		MessageHandler::addNotification(g_cameraEnabled ? "Camera enabled" : "Camera disabled");
	}
}
