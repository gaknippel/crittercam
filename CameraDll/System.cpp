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
#include "PathOverlay.h"
#include "Studio.h"
#include <timeapi.h>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

using namespace DirectX;

extern "C" LPBYTE g_cameraOverrideBuffer;
extern "C" LPBYTE g_gameSourceCamera;

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
			Sleep(FRAME_SLEEP);
			updateFrame();
		}
	}


	// updates the data and camera for a frame
	void System::updateFrame()
	{
		handleUserInput();
		followWorldShift();
		if (g_cameraEnabled && _cameraPath.isPlaying())
		{
			XMFLOAT3 position;
			XMVECTOR rotation;
			float fov;
			bool stillPlaying = _cameraPath.evaluate(position, rotation, fov);
			if (Globals::instance().settings().trackPlayer && Globals::instance().settings().trackDuringDolly)
			{
				// the dolly moves the camera, tracking aims it (so after playback it stays at the end of the path, still tracking)
				_camera.setPosition(position);
				if (!_trackActive)
				{
					// start from the path's current view, so turning tracking on mid-path doesn't snap
					XMFLOAT3 forward;
					XMStoreFloat3(&forward, XMMatrixRotationQuaternion(rotation).r[2]);
					_camera.setYaw(atan2f(forward.x, forward.z));
					_camera.setPitch(asinf(std::clamp(-forward.y, -1.0f, 1.0f)));
				}
				updateTracking(false);
				if (_trackActive)
				{
					rotation = _camera.calculateRotationQuaternion();
				}
			}
			CameraManipulator::writeCameraPose(position, rotation, fov);
			if (!stillPlaying)
			{
				MessageHandler::addNotification("Dolly playback finished");
			}
		}
		else
		{
			updateTracking(Globals::instance().settings().trackFollow);
			CameraManipulator::updateCameraDataInGameData(_camera);
		}
		if (g_cameraEnabled)
		{
			// the game moves the character relative to its own camera: keep that one facing where we look (keyboard and pads alike)
			Studio::alignGameCameraYaw(CameraManipulator::cameraYaw(g_cameraOverrideBuffer));
		}
		updateGameSpeed();
		Studio::update();
		// path overlay: while flying the free cam, hidden during playback and with the HUD (so one key gives a clean frame).
		const bool showPath = g_cameraEnabled && !_cameraPath.isPlaying() && _cameraPath.nodeCount() > 0 && Globals::instance().hudVisible();
		if (showPath)
		{
			_cameraPath.sampleForDisplay(_pathCurve, _pathNodes, _pathAims);
			PathOverlay::update(_pathCurve, _pathNodes, _pathAims);
		}
		PathOverlay::setVisible(showPath);
		if (GetTickCount64() >= _nextStateSend)
		{
			_nextStateSend = GetTickCount64() + STATE_SEND_INTERVAL_MS;
			sendCameraState();
		}
	}


	// false when the camera can't be read (freed after a loading screen)
	static bool readCameraPosition(LPBYTE camera, XMFLOAT3& position)
	{
		__try
		{
			memcpy(&position, camera + CAMERA_POSITION_IN_STRUCT_OFFSET, sizeof(XMFLOAT3));
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	// Far from where it started, the game re-centres its world coordinates around the player (like Elden Ring's map tiles):
	// everything jumps by the same offset in one frame, so our camera's unchanged numbers suddenly point somewhere else.
	// The game's own camera jumps with the world, so when it jumps further than anything moves in one update, move our
	// camera and the dolly path by the same offset.
	void System::followWorldShift()
	{
		static LPBYTE lastCamera = nullptr;
		static XMFLOAT3 last, velocity;		// velocity: how far the game camera moved last frame, so its own motion isn't counted as shift
		LPBYTE camera = g_cameraEnabled ? g_gameSourceCamera : nullptr;
		XMFLOAT3 now;
		if (nullptr == camera || !readCameraPosition(camera, now))
		{
			lastCamera = nullptr;
			return;
		}
		if (camera == lastCamera && now.x == last.x && now.y == last.y && now.z == last.z)
		{
			return;		// we run every ms, the game updates once a frame: only compare frames where it moved
		}
		if (camera != lastCamera)		// a different camera object (or none) in between isn't a shift
		{
			velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
		}
		else
		{
			XMFLOAT3 shift(now.x - last.x - velocity.x, now.y - last.y - velocity.y, now.z - last.z - velocity.z);
			const float distance = sqrtf(shift.x * shift.x + shift.y * shift.y + shift.z * shift.z);
			velocity = XMFLOAT3(now.x - last.x, now.y - last.y, now.z - last.z);
			if (distance > WORLD_SHIFT_MIN_METERS)
			{
				velocity = XMFLOAT3(velocity.x - shift.x, velocity.y - shift.y, velocity.z - shift.z);
				const XMFLOAT3 position = _camera.getPosition();
				_camera.setPosition(XMFLOAT3(position.x + shift.x, position.y + shift.y, position.z + shift.z));
				_cameraPath.shift(shift);
				_trackAnchor = XMFLOAT3(_trackAnchor.x + shift.x, _trackAnchor.y + shift.y, _trackAnchor.z + shift.z);
				MessageHandler::logLine("The game shifted its world coordinates by (%.1f, %.1f, %.1f), the camera followed", shift.x, shift.y, shift.z);
			}
		}
		lastCamera = camera;
		last = now;
	}


	// yaw / pitch that look from 'from' at 'to', in the camera's convention: forward = (sin(yaw)cos(pitch), -sin(pitch), cos(yaw)cos(pitch))
	static void lookAngles(const XMFLOAT3& from, const XMFLOAT3& to, float& yaw, float& pitch)
	{
		const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
		yaw = atan2f(dx, dz);
		pitch = atan2f(-dy, sqrtf(dx * dx + dz * dz));
	}


	// the camera keeps its angles in 0..2pi: differences go back to -pi..pi
	static float signedAngle(float angle)
	{
		angle = fmodf(angle + XM_PI, XM_2PI);
		return (angle < 0.0f ? angle + XM_2PI : angle) - XM_PI;
	}


	// Tracking camera: keeps the player in frame like a camera operator, not an aimbot. The camera looks at an anchor point
	// that chases the player's chest with an exponential lag, so quick moves get a lazy catch-up. It starts from the shot you
	// have (an off-centre framing stays off-centre), and manual looking / flying while it tracks changes the framing / distance.
	// With follow on, the camera also moves along, keeping its offset to the anchor. During dolly playback the path moves it instead.
	void System::updateTracking(bool follow)
	{
		const Settings& settings = Globals::instance().settings();
		float feet[3];
		if (!g_cameraEnabled || !settings.trackPlayer || !Studio::localPlayerPosition(feet))
		{
			_trackActive = false;
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		const XMFLOAT3 target(feet[0], feet[1] + TRACK_TARGET_HEIGHT, feet[2]);
		XMFLOAT3 position = _camera.getPosition();
		float yaw, pitch;
		if (!_trackActive)
		{
			_trackActive = true;
			_trackAnchor = target;
			_trackOffset = XMFLOAT3(position.x - target.x, position.y - target.y, position.z - target.z);
			lookAngles(position, target, yaw, pitch);
			_trackYawOffset = signedAngle(_camera.getYaw() - yaw);
			_trackPitchOffset = signedAngle(_camera.getPitch() - pitch);
			_trackLastYaw = _camera.getYaw();
			_trackLastPitch = _camera.getPitch();
			_trackLastUpdate = now;
			return;
		}
		// whatever the user turned / flew since the last update becomes part of the framing
		_trackYawOffset = signedAngle(_trackYawOffset + _camera.getYaw() - _trackLastYaw);
		_trackPitchOffset += signedAngle(_camera.getPitch() - _trackLastPitch);
		if (follow)
		{
			_trackOffset = XMFLOAT3(position.x - _trackAnchor.x, position.y - _trackAnchor.y, position.z - _trackAnchor.z);
		}
		const float seconds = (std::min)(std::chrono::duration<float>(now - _trackLastUpdate).count(), 0.25f);
		_trackLastUpdate = now;
		const float blend = 1.0f - expf(-seconds / (std::max)(settings.trackLagSeconds, TRACK_LAG_SECONDS_MIN));
		_trackAnchor = XMFLOAT3(_trackAnchor.x + (target.x - _trackAnchor.x) * blend, _trackAnchor.y + (target.y - _trackAnchor.y) * blend,
								_trackAnchor.z + (target.z - _trackAnchor.z) * blend);
		if (follow)
		{
			position = XMFLOAT3(_trackAnchor.x + _trackOffset.x, _trackAnchor.y + _trackOffset.y, _trackAnchor.z + _trackOffset.z);
			_camera.setPosition(position);
		}
		lookAngles(position, _trackAnchor, yaw, pitch);
		_trackPitchOffset = std::clamp(pitch + _trackPitchOffset, -PITCH_LIMIT, PITCH_LIMIT) - pitch;		// so it can't wind past straight up / down
		_camera.setYaw(yaw + _trackYawOffset);
		_camera.setPitch(pitch + _trackPitchOffset);
		_trackLastYaw = _camera.getYaw();
		_trackLastPitch = _camera.getPitch();
	}


	// Reads one character. Characters get freed under us (loading screens, deaths), so every read is guarded: a bad pointer just skips it.
	// The vtable must point into the game's image, which filters out the floats that follow the character array.
	static bool readCharacter(uintptr_t chr, uintptr_t imageStart, uintptr_t imageEnd, float* position, int* hp, uintptr_t* vtable)
	{
		__try
		{
			const uintptr_t vt = *reinterpret_cast<uintptr_t*>(chr);
			if (vt < imageStart || vt >= imageEnd)
			{
				return false;
			}
			const uintptr_t modules = *reinterpret_cast<uintptr_t*>(chr + CHR_MODULES_OFFSET);
			const uintptr_t data = *reinterpret_cast<uintptr_t*>(modules + CHR_DATA_MODULE_OFFSET);
			const uintptr_t physics = *reinterpret_cast<uintptr_t*>(modules + CHR_PHYSICS_MODULE_OFFSET);
			memcpy(position, reinterpret_cast<void*>(physics + CHR_POSITION_OFFSET), 3 * sizeof(float));
			*hp = *reinterpret_cast<int*>(data + CHR_HP_OFFSET);
			*vtable = vt;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	static bool readPointer(uintptr_t address, uintptr_t* value)
	{
		__try
		{
			*value = *reinterpret_cast<uintptr_t*>(address);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	// JSON snapshot for the client's "in game" tab: the rendered camera (only while ours is on), the dolly nodes and the curve.
	void System::sendCameraState()
	{
		char number[160];
		std::string json;
		snprintf(number, sizeof(number), "{\"on\":%d,\"paused\":%d,\"playing\":%d,\"hud\":%d,\"loop\":%d,\"speed\":%.3f,\"cam\":",
				 g_cameraEnabled ? 1 : 0, Globals::instance().gamePaused() ? 1 : 0, _cameraPath.isPlaying() ? 1 : 0,
				 Globals::instance().hudVisible() ? 1 : 0, Globals::instance().settings().dollyLoop ? 1 : 0, _appliedGameSpeed);
		json += number;
		if (g_cameraEnabled)
		{
			// the buffer the game renders from, so this is also right during dolly playback.
			const float* m = reinterpret_cast<const float*>(g_cameraOverrideBuffer + CAMERA_MATRIX_IN_STRUCT_OFFSET);
			const float fov = *reinterpret_cast<const float*>(g_cameraOverrideBuffer + FOV_IN_STRUCT_OFFSET);
			snprintf(number, sizeof(number), "[%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f]", m[12], m[13], m[14],
					 XMConvertToDegrees(atan2f(m[8], m[10])), XMConvertToDegrees(asinf(std::clamp(-m[9], -1.0f, 1.0f))),
					 XMConvertToDegrees(atan2f(m[1], m[5])), XMConvertToDegrees(fov));
			json += number;
		}
		else
		{
			json += "null";
		}
		_cameraPath.sampleForDisplay(_pathCurve, _pathNodes, _pathAims);
		json += ",\"nodes\":[";
		for (size_t i = 0; i < _pathNodes.size(); i++)
		{
			const XMFLOAT3& p = _pathNodes[i];
			const XMFLOAT3 f(_pathAims[i].x - p.x, _pathAims[i].y - p.y, _pathAims[i].z - p.z);	// aim is 1m along the view direction
			snprintf(number, sizeof(number), "%s[%.2f,%.2f,%.2f,%.1f,%.1f,%.1f]", i ? "," : "", p.x, p.y, p.z,
					 XMConvertToDegrees(atan2f(f.x, f.z)), XMConvertToDegrees(asinf(std::clamp(-f.y, -1.0f, 1.0f))), XMConvertToDegrees(_cameraPath.nodeFoV(static_cast<int>(i))));
			json += number;
		}
		json += "],\"curve\":[";
		// every 3rd sample is plenty for a small map, and keeps the message well under the pipe's 64KB.
		for (size_t i = 0; i < _pathCurve.size(); i += 3)
		{
			snprintf(number, sizeof(number), "%s[%.2f,%.2f,%.2f]", i ? "," : "", _pathCurve[i].x, _pathCurve[i].y, _pathCurve[i].z);
			json += number;
		}
		if (!_pathCurve.empty())
		{
			const XMFLOAT3& last = _pathCurve.back();
			snprintf(number, sizeof(number), ",[%.2f,%.2f,%.2f]", last.x, last.y, last.z);
			json += number;
		}
		json += "]";
		// characters: "me":[x,y,z] (the local player), "chrs":[[x,y,z,isPlayer],...] (everyone else alive)
		uintptr_t worldChrMan = 0, me = 0;
		const uintptr_t imageStart = reinterpret_cast<uintptr_t>(_hostImageAddress), imageEnd = imageStart + _hostImageSize;
		float position[3];
		int hp;
		uintptr_t meVtable = 0;
		if (nullptr != _worldChrManPointer && readPointer(reinterpret_cast<uintptr_t>(_worldChrManPointer), &worldChrMan) && 0 != worldChrMan
			&& readPointer(worldChrMan + WORLD_CHR_MAN_MAIN_PLAYER_OFFSET, &me) && 0 != me && readCharacter(me, imageStart, imageEnd, position, &hp, &meVtable))
		{
			snprintf(number, sizeof(number), ",\"me\":[%.2f,%.2f,%.2f],\"chrs\":[", position[0], position[1], position[2]);
			json += number;
			bool first = true;
			uintptr_t begin = 0, end = 0;
			readPointer(worldChrMan + WORLD_CHR_MAN_CHARACTERS_BEGIN_OFFSET, &begin);
			readPointer(worldChrMan + WORLD_CHR_MAN_CHARACTERS_END_OFFSET, &end);
			const int count = end > begin ? (std::min)(static_cast<int>((end - begin) / sizeof(uintptr_t)), MAX_CHARACTERS) : 0;
			for (int i = 0; i < count; i++)
			{
				uintptr_t chr = 0, vtable = 0;
				if (!readPointer(begin + i * sizeof(uintptr_t), &chr) || 0 == chr || !readCharacter(chr, imageStart, imageEnd, position, &hp, &vtable))
				{
					continue;
				}
				if (chr == me || hp <= 0)
				{
					continue;
				}
				snprintf(number, sizeof(number), "%s[%.2f,%.2f,%.2f,%d]", first ? "" : ",", position[0], position[1], position[2], vtable == meVtable ? 1 : 0);
				json += number;
				first = false;
			}
			json += "]";
		}
		json += "}";
		NamedPipeManager::instance().writeTextPayload(json, MessageType::CameraState);
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
		// held keys / sticks move by elapsed time, so the speed doesn't depend on how often this loop runs. Clamped so a stall doesn't teleport the camera.
		auto now = std::chrono::steady_clock::now();
		float frameScale = (std::min)(std::chrono::duration<float>(now - _lastInputTime).count() / SPEED_REFERENCE_SECONDS, 10.0f);
		_lastInputTime = now;
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
		handleGameSpeedWheel();

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
		if (Input::isActionActivated(ActionType::TrackPlayer))
		{
			Settings& settings = Globals::instance().settings();
			settings.trackPlayer = !settings.trackPlayer;
			NamedPipeManager::instance().writeTextPayload(Utils::formatString("%d=%d", static_cast<int>(SettingType::TrackPlayer), settings.trackPlayer ? 1 : 0), MessageType::SettingChanged);
			MessageHandler::addNotification(settings.trackPlayer ? "Tracking you" : "Tracking off");
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
			_camera.changeFoV(-Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE * frameScale);
		}
		if (Input::isActionActivated(ActionType::FovIncrease) && Globals::instance().keyboardMouseControlCamera())
		{
			_camera.changeFoV(Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE * frameScale);
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
			// no movement allowed, simply return (dropping the mouse movement, or unlocking would apply it all at once)
			Input::resetMouseDeltas();
			return;
		}

		float multiplier = Utils::altPressed() ? settings.fastMovementMultiplier : Utils::ctrlPressed() ? settings.slowMovementMultiplier : 1.0f;
		// Zoomed in = move slower, so it stays controllable. Relative to the fov the game had when the camera was enabled.
		multiplier *= std::clamp(fabsf(_camera.getFoV()) / _camera.getOriginalFoV(), 0.01f, 1.0f);
		handleKeyboardCameraMovement(multiplier * frameScale);
		handleMouseCameraMovement(multiplier);		// mouse deltas are already per-update distances, no time scaling
		handleGamePadMovement(multiplier, frameScale);
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
			// the mouse moved the game camera while ours was off and those deltas piled up: applied now, they'd spin the
			// camera to a random angle on its first frame.
			Input::resetMouseDeltas();
			g_cameraEnabled = 1;
		}
		displayCameraState();
	}


	void System::handleGamePadMovement(float multiplierBase, float frameScale)
	{
		if(!Globals::instance().controllerControlsCamera())
		{
			return;
		}

		auto gamePad = Globals::instance().gamePad();
		static bool missingPadReported = false;
		if (!gamePad.isConnected() && !missingPadReported)
		{
			// we read the pad through XInput: PlayStation/Switch pads only show up via Steam Input or DS4Windows.
			MessageHandler::addNotification("No XInput controller found. PlayStation pad? Enable Steam Input or use DS4Windows.");
			missingPadReported = true;
		}

		if (gamePad.isConnected())
		{
			Settings& settings = Globals::instance().settings();
			float  multiplier = gamePad.isButtonPressed(IGCS_BUTTON_FASTER) ? settings.fastMovementMultiplier
																			: gamePad.isButtonPressed(IGCS_BUTTON_SLOWER) ? settings.slowMovementMultiplier : multiplierBase;
			multiplier *= frameScale;
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
				_camera.changeFoV(-Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE * frameScale);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_FOV_INCREASE))
			{
				_camera.changeFoV(Globals::instance().settings().fovChangeSpeed * FOV_CHANGE_SCALE * frameScale);
			}
		}
	}


	void System::handleMouseCameraMovement(float multiplier)
	{
		if (!Globals::instance().keyboardMouseControlCamera())
		{
			Input::resetMouseDeltas();		// the mouse is playing the game: don't let it pile up for when it controls the camera again
			return;
		}
		long mouseDeltaX = Input::getMouseDeltaX();
		long mouseDeltaY = Input::getMouseDeltaY();
		bool leftButtonPressed = Input::isMouseButtonDown(0);
		bool rightButtonPressed = Input::isMouseButtonDown(1);
		bool noButtonPressed = !(leftButtonPressed || rightButtonPressed);
		if (mouseDeltaY != 0)
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
		if (mouseDeltaX != 0)
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
		PathOverlay::start();
		Input::registerRawInput();

		GameSpecific::InterceptorHelper::initializeAOBBlocks(_hostImageAddress, _hostImageSize, _aobBlocks);
		GameSpecific::InterceptorHelper::setCameraStructInterceptorHook(_aobBlocks);
		waitForCameraStructAddresses();		// blocks till camera is found.
		GameSpecific::InterceptorHelper::setPostCameraStructHooks(_aobBlocks);
		if (nullptr != _aobBlocks[WORLD_CHR_MAN_LOCATION_KEY]->locationInImage())
		{
			_worldChrManPointer = Utils::calculateAbsoluteAddress(_aobBlocks[WORLD_CHR_MAN_LOCATION_KEY], 4);
		}
		Studio::initialize(_worldChrManPointer, _aobBlocks[PLAY_ANIMATION_KEY]->locationInImage());
		AOBBlock* flagMan = _aobBlocks[EVENT_FLAG_MAN_LOCATION_KEY];
		Studio::initializeEventFlags(nullptr == flagMan->locationInImage() ? nullptr : Utils::calculateAbsoluteAddress(flagMan, 4),
									 _aobBlocks[GET_EVENT_FLAG_KEY]->locationInImage(), _aobBlocks[SET_EVENT_FLAG_KEY]->locationInImage());

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


	// Pause wins (instant, so frame skipping works). Otherwise ease toward the speed setting so speed changes look smooth on video.
	// Only writes while the speed isn't normal (and once more to restore 1.0), so the game's own speed effects aren't overridden.
	void System::updateGameSpeed()
	{
		const auto now = std::chrono::steady_clock::now();
		const float seconds = std::chrono::duration<float>(now - _lastSpeedUpdate).count();
		_lastSpeedUpdate = now;
		const float target = Globals::instance().gamePaused() ? GAME_SPEED_PAUSED : Globals::instance().settings().gameSpeed;
		float next = target;
		if (!Globals::instance().gamePaused() && _appliedGameSpeed > 0.0f && GAME_SPEED_EASE_SECONDS > 0.0f)
		{
			next = _appliedGameSpeed + (target - _appliedGameSpeed) * (std::min)(1.0f, seconds / GAME_SPEED_EASE_SECONDS);
			if (fabsf(next - target) < 0.002f)
			{
				next = target;
			}
		}
		if (next == 1.0f && _appliedGameSpeed == 1.0f)
		{
			return;
		}
		if (CameraManipulator::setGameSpeed(next))
		{
			_appliedGameSpeed = next;
		}
	}


	// fixed steps on a grid (rounding first snaps an off-grid slider value), so scrolling always lands on round numbers like 1.0
	static float stepOnGrid(float value, short notches, float step, float min, float max)
	{
		return std::clamp((roundf(value / step) + notches) * step, min, max);
	}


	// shift + mouse wheel changes the game speed, ctrl + mouse wheel the dolly seconds per node, camera on or off.
	// The client hears about it and updates its slider.
	void System::handleGameSpeedWheel()
	{
		const short wheel = Input::getMouseWheelDelta();
		if (0 == wheel)
		{
			return;
		}
		Settings& settings = Globals::instance().settings();
		const bool shift = Utils::shiftPressed();
		const bool ctrl = !shift && Utils::ctrlPressed();
		if (shift)
		{
			// While paused this only changes the setting: unpausing resumes at it.
			settings.gameSpeed = stepOnGrid(settings.gameSpeed, wheel, GAME_SPEED_WHEEL_STEP, GAME_SPEED_WHEEL_MIN, GAME_SPEED_MAX);
			NamedPipeManager::instance().writeTextPayload(Utils::formatString("%d=%.3f", static_cast<int>(SettingType::GameSpeed), settings.gameSpeed), MessageType::SettingChanged);
		}
		if (ctrl)
		{
			settings.dollySegmentSeconds = stepOnGrid(settings.dollySegmentSeconds, wheel, DOLLY_SECONDS_WHEEL_STEP, DOLLY_SECONDS_WHEEL_STEP, DOLLY_SEGMENT_SECONDS_MAX);
			NamedPipeManager::instance().writeTextPayload(Utils::formatString("%d=%.3f", static_cast<int>(SettingType::DollySegmentSeconds), settings.dollySegmentSeconds), MessageType::SettingChanged);
			MessageHandler::addNotification(Utils::formatString("%.1f seconds per node", settings.dollySegmentSeconds));
		}
		if (shift || ctrl || !g_cameraEnabled)
		{
			// consumed: shift / ctrl + wheel shouldn't also zoom, and with the camera off nothing else reads the wheel, so don't let it pile up.
			Input::resetMouseDeltas();
		}
	}


	void System::toggleGamePause(bool displayNotification)
	{
		bool gamePaused = Globals::instance().toggleGamePaused();
		if (!CameraManipulator::setGameSpeed(gamePaused ? GAME_SPEED_PAUSED : Globals::instance().settings().gameSpeed))
		{
			Globals::instance().toggleGamePaused();		// didn't work, flip our state back
			MessageHandler::addNotification("Pause isn't available right now");
			return;
		}
		_appliedGameSpeed = gamePaused ? GAME_SPEED_PAUSED : Globals::instance().settings().gameSpeed;
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
