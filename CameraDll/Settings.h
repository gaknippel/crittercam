////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include "Gamepad.h"
#include "Utils.h"
#include "GameConstants.h"
#include "Defaults.h"
#include <map>
#include "ActionData.h"

namespace IGCS
{
	struct Settings
	{
		bool invertY;
		float fastMovementMultiplier;
		float slowMovementMultiplier;
		float movementUpMultiplier;
		float movementSpeed;
		float rotationSpeed;
		float fovChangeSpeed;
		int cameraControlDevice;		// 0==keyboard/mouse, 1 == gamepad, 2 == both, see Defaults.h
		float dollySegmentSeconds;		// seconds the dolly takes from one node to the next
		bool dollyLoop;					// loop the dolly path (last node flows back into the first)

		void setValueFromMessage(uint8_t payload[], DWORD payloadLength)
		{
			// byte 1 is the id of the setting. Bytes 2 and further contain the data for the setting.
			if(payloadLength<3)
			{
				return;
			}
			switch(static_cast<SettingType>(payload[1]))
			{
			case SettingType::FastMovementMultiplier:
				fastMovementMultiplier = Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::SlowMovementMultiplier:
				slowMovementMultiplier = Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::UpMovementMultiplier:
				movementUpMultiplier= Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::MovementSpeed:
				movementSpeed= Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::CameraControlDevice:
				cameraControlDevice = Utils::intFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::RotationSpeed:
				rotationSpeed = Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::InvertYLookDirection:
				invertY = payload[2] == (uint8_t)1 ? true : false;
				break;
			case SettingType::FoVZoomSpeed:
				fovChangeSpeed= Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::DollySegmentSeconds:
				dollySegmentSeconds = Utils::floatFromBytes(payload, payloadLength, 2);
				break;
			case SettingType::DollyLoop:
				dollyLoop = payload[2] == (uint8_t)1 ? true : false;
				break;
			default:
				// nothing
				break;
			}
		}


		void init(bool persistedOnly)
		{
			invertY = CONTROLLER_Y_INVERT;
			fastMovementMultiplier = FASTER_MULTIPLIER;
			slowMovementMultiplier = SLOWER_MULTIPLIER;
			movementUpMultiplier = DEFAULT_UP_MOVEMENT_MULTIPLIER;
			movementSpeed = DEFAULT_MOVEMENT_SPEED;
			rotationSpeed = DEFAULT_ROTATION_SPEED;
			fovChangeSpeed = DEFAULT_FOV_SPEED;
			cameraControlDevice = DEVICE_ID_ALL;
			dollySegmentSeconds = DEFAULT_DOLLY_SEGMENT_SECONDS;
			dollyLoop = false;
		}
	};
}
