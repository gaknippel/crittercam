////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "CameraManipulator.h"
#include "GameConstants.h"
#include "Globals.h"
#include "Camera.h"
#include "MessageHandler.h"
#include <cmath>

using namespace DirectX;
using namespace std;

namespace
{
	// Two copies of the camera struct. We fill the one the game isn't reading, then flip the pointer, so the game never
	// copies a half-written matrix. The game copies with movups, but keep them aligned anyway.
	alignas(16) uint8_t s_overrideBuffers[2][CAMERA_STRUCT_SIZE] = {};
}

//--------------------------------------------------------------------------------------------------------------------------------
// data shared with asm functions.
extern "C" {
	LPBYTE g_cameraStructAddress = nullptr;					// last destination camera the routine wrote to (while our camera is off)
	LPBYTE g_activeCameraStructAddress = nullptr;			// the destination camera we override
	LPBYTE g_cameraOverrideBuffer = s_overrideBuffers[0];	// the buffer the asm points the routine's source at
	LPBYTE g_cameraCandidates[MAX_CAMERA_CANDIDATES] = {};	// distinct destinations seen, filled by the asm
	LPBYTE g_gameSourceCamera = nullptr;					// the game's own camera while ours is on, set by the asm. Character input is relative to it.
}

namespace IGCS::GameSpecific::CameraManipulator
{
	static LPBYTE _timeDilationPointerAddress = nullptr;	// address of the global pointer to the time dilation struct
	static int _pausedFloatOffset = -1;						// which float we zeroed to pause, -1 if not paused
	static float _valueBeforePause = 1.0f;


	bool isCameraFound()
	{
		return nullptr != g_cameraStructAddress;
	}


	void displayCameraStructAddress()
	{
		MessageHandler::logDebug("Camera struct address: %p", (void*)g_cameraStructAddress);
	}


	static int countCandidates()
	{
		int count = 0;
		for (int i = 0; i < MAX_CAMERA_CANDIDATES; i++)
		{
			if (nullptr != g_cameraCandidates[i])
			{
				count++;
			}
		}
		return count;
	}


	static void logCandidates()
	{
		int count = countCandidates();
		MessageHandler::logDebug("Camera routine writes to %d camera(s):", count);
		for (int i = 0; i < MAX_CAMERA_CANDIDATES; i++)
		{
			if (nullptr != g_cameraCandidates[i])
			{
				MessageHandler::logDebug("  %d: %p%s", i + 1, (void*)g_cameraCandidates[i], g_cameraCandidates[i] == g_activeCameraStructAddress ? "  <<< active" : "");
			}
		}
		if (count > 1)
		{
			MessageHandler::addNotification("Several game cameras found. If the view doesn't move, press 'Cycle camera target'");
		}
	}


	static void initializeBuffersFromGameCamera(LPBYTE gameCamera, Camera& camera)
	{
		memcpy(s_overrideBuffers[0], gameCamera, CAMERA_STRUCT_SIZE);
		memcpy(s_overrideBuffers[1], gameCamera, CAMERA_STRUCT_SIZE);
		g_cameraOverrideBuffer = s_overrideBuffers[0];
		const float* matrix = reinterpret_cast<const float*>(gameCamera + CAMERA_MATRIX_IN_STRUCT_OFFSET);
		const float fov = *reinterpret_cast<const float*>(gameCamera + FOV_IN_STRUCT_OFFSET);
		camera.initializeFromGameMatrix(matrix, (fov > FOV_MIN && fov < FOV_MAX) ? fov : DEFAULT_FOV);
	}


	bool takeOverGameCamera(Camera& camera)
	{
		// Collect the destinations fresh: old entries could point to cameras that were freed (e.g. after a loading screen).
		LPBYTE previousTarget = g_activeCameraStructAddress;
		for (int i = 0; i < MAX_CAMERA_CANDIDATES; i++)
		{
			g_cameraCandidates[i] = nullptr;
		}
		g_cameraStructAddress = nullptr;
		g_gameSourceCamera = nullptr;		// may point to a freed camera from last time; the asm sets it again once ours is on
		Sleep(100);		// a few frames, the asm fills the table again
		if (nullptr == g_cameraStructAddress)
		{
			MessageHandler::addNotification("No game camera found right now (loading screen?)");
			return false;
		}

		// Prefer the camera picked last time if it's still in use, otherwise the last one written this frame.
		LPBYTE target = g_cameraStructAddress;
		for (int i = 0; i < MAX_CAMERA_CANDIDATES; i++)
		{
			if (nullptr != previousTarget && g_cameraCandidates[i] == previousTarget)
			{
				target = previousTarget;
				break;
			}
		}
		initializeBuffersFromGameCamera(target, camera);
		writeCameraPose(camera.getPosition(), camera.calculateRotationQuaternion(), camera.getFoV());
		g_activeCameraStructAddress = target;		// must be set before g_cameraEnabled, which the caller sets.
		logCandidates();
		return true;
	}


	void releaseGameCamera()
	{
		// Nothing to restore: the game copies its own camera again as soon as g_cameraEnabled is 0.
	}


	void cycleCameraTarget()
	{
		int count = countCandidates();
		if (count < 2)
		{
			MessageHandler::addNotification("Only one game camera found, nothing to cycle");
			return;
		}
		int current = -1;
		for (int i = 0; i < MAX_CAMERA_CANDIDATES; i++)
		{
			if (g_cameraCandidates[i] == g_activeCameraStructAddress)
			{
				current = i;
				break;
			}
		}
		for (int step = 1; step <= MAX_CAMERA_CANDIDATES; step++)
		{
			int next = (current + step) % MAX_CAMERA_CANDIDATES;
			if (nullptr != g_cameraCandidates[next])
			{
				g_activeCameraStructAddress = g_cameraCandidates[next];
				MessageHandler::addNotification(Utils::formatString("Camera target %d of %d", next + 1, count));
				logCandidates();
				return;
			}
		}
	}


	void writeCameraPose(const XMFLOAT3& position, FXMVECTOR rotationQuaternion, float fov)
	{
		XMMATRIX rotation = XMMatrixRotationQuaternion(rotationQuaternion);
		LPBYTE current = g_cameraOverrideBuffer;
		LPBYTE next = (current == s_overrideBuffers[0]) ? s_overrideBuffers[1] : s_overrideBuffers[0];
		// start from the current buffer so aspect/near/far (kept live by the asm) and the w components carry over.
		memcpy(next, current, CAMERA_STRUCT_SIZE);
		float* matrix = reinterpret_cast<float*>(next + CAMERA_MATRIX_IN_STRUCT_OFFSET);
		XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&matrix[0]), rotation.r[0]);	// right
		XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&matrix[4]), rotation.r[1]);	// up
		XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&matrix[8]), rotation.r[2]);	// forward
		matrix[12] = position.x;
		matrix[13] = position.y;
		matrix[14] = position.z;
		*reinterpret_cast<float*>(next + FOV_IN_STRUCT_OFFSET) = fov;
		InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_cameraOverrideBuffer), next);
	}


	void updateCameraDataInGameData(Camera& camera)
	{
		if (!g_cameraEnabled)
		{
			return;
		}
		camera.applyMovement();
		writeCameraPose(camera.getPosition(), camera.calculateRotationQuaternion(), camera.getFoV());
	}


	void applySettingsToGameState()
	{
		// All current settings (speeds, dolly timing) are read live by the camera code.
	}


	void setTimeDilationPointerAddress(LPBYTE address)
	{
		_timeDilationPointerAddress = address;
		MessageHandler::logDebug("Time dilation pointer address: %p", (void*)address);
	}


	static float* getTimeDilationFloat(int offset)
	{
		if (nullptr == _timeDilationPointerAddress)
		{
			return nullptr;
		}
		// Re-read the pointer every time: the struct could be re-allocated, e.g. after a loading screen.
		LPBYTE timeDilationStruct = *reinterpret_cast<LPBYTE*>(_timeDilationPointerAddress);
		if (nullptr == timeDilationStruct)
		{
			return nullptr;
		}
		return reinterpret_cast<float*>(timeDilationStruct + offset);
	}


	bool setTimeStopValue(bool pauseGame)
	{
		if (pauseGame)
		{
			float* float1 = getTimeDilationFloat(TIME_DILATION_FLOAT1_OFFSET);
			float* float2 = getTimeDilationFloat(TIME_DILATION_FLOAT2_OFFSET);
			if (nullptr == float1 || nullptr == float2)
			{
				MessageHandler::addNotification("Pause isn't available right now");
				return false;
			}
			// The game multiplies these two each frame: one is the game speed (normally exactly 1.0), the other the frame delta.
			// Zero the game speed one.
			float value1 = *float1;
			float value2 = *float2;
			if (value1 == 1.0f || (value2 != 1.0f && fabsf(value1 - 1.0f) <= fabsf(value2 - 1.0f)))
			{
				_pausedFloatOffset = TIME_DILATION_FLOAT1_OFFSET;
				_valueBeforePause = value1;
				*float1 = 0.0f;
			}
			else
			{
				_pausedFloatOffset = TIME_DILATION_FLOAT2_OFFSET;
				_valueBeforePause = value2;
				*float2 = 0.0f;
			}
			MessageHandler::logDebug("Pause: +0x%X was %f, +0x%X was %f. Zeroed +0x%X.", TIME_DILATION_FLOAT1_OFFSET, value1,
									 TIME_DILATION_FLOAT2_OFFSET, value2, _pausedFloatOffset);
		}
		else
		{
			if (_pausedFloatOffset < 0)
			{
				return true;
			}
			float* pausedFloat = getTimeDilationFloat(_pausedFloatOffset);
			if (nullptr != pausedFloat)
			{
				*pausedFloat = _valueBeforePause > 0.0f ? _valueBeforePause : 1.0f;
			}
			_pausedFloatOffset = -1;
		}
		return true;
	}


	// Called every frame while paused, in case the game writes the value back.
	void reassertTimeStop()
	{
		if (_pausedFloatOffset < 0)
		{
			return;
		}
		float* pausedFloat = getTimeDilationFloat(_pausedFloatOffset);
		if (nullptr != pausedFloat)
		{
			*pausedFloat = 0.0f;
		}
	}
}
