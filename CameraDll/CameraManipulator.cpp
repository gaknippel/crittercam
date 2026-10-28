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
	static int _speedFloatOffset = -1;						// which time dilation float is the game speed, -1 until first used


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


	// forward - forward.y * up stays valid when looking straight up or down.
	float cameraYaw(LPBYTE camera)
	{
		const float* up = reinterpret_cast<const float*>(camera + CAMERA_MATRIX_IN_STRUCT_OFFSET + 0x10);
		const float* forward = reinterpret_cast<const float*>(camera + CAMERA_MATRIX_IN_STRUCT_OFFSET + 0x20);
		return atan2f(forward[0] - forward[1] * up[0], forward[2] - forward[1] * up[2]);
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


	// The game multiplies two floats each frame: the game speed (normally exactly 1.0) and the frame delta. Returns the speed one.
	static float* getGameSpeedFloat()
	{
		float* float1 = getTimeDilationFloat(TIME_DILATION_FLOAT1_OFFSET);
		float* float2 = getTimeDilationFloat(TIME_DILATION_FLOAT2_OFFSET);
		if (nullptr == float1 || nullptr == float2)
		{
			return nullptr;
		}
		if (_speedFloatOffset < 0)
		{
			// decided once, the first time it's needed: the speed float is the one at (or closest to) 1.0
			_speedFloatOffset = fabsf(*float1 - 1.0f) <= fabsf(*float2 - 1.0f) ? TIME_DILATION_FLOAT1_OFFSET : TIME_DILATION_FLOAT2_OFFSET;
			MessageHandler::logLine("Game speed: +0x%X was %f, +0x%X was %f. Using +0x%X.", TIME_DILATION_FLOAT1_OFFSET, *float1,
									TIME_DILATION_FLOAT2_OFFSET, *float2, _speedFloatOffset);
		}
		return getTimeDilationFloat(_speedFloatOffset);
	}


	bool setGameSpeed(float speed)
	{
		float* speedFloat = getGameSpeedFloat();
		if (nullptr == speedFloat)
		{
			return false;
		}
		*speedFloat = speed;
		return true;
	}
}
