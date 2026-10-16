////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2020, Frans Bouma
// All rights reserved.
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "stdafx.h"
#include "Gamepad.h"

namespace IGCS
{
	// System defaults
	#define FRAME_SLEEP								1		// in milliseconds. Fast loop so the camera updates every rendered frame; movement is scaled by real elapsed time.
	#define SPEED_REFERENCE_SECONDS					0.008f	// speed settings are 'per 8ms tick' (the old loop rate), keyboard/gamepad input is scaled to that
	#define IGCS_SUPPORT_RAWKEYBOARDINPUT			true	// if set to false, raw keyboard input is ignored.
	#define IGCS_MAX_MESSAGE_SIZE					4*1024	// in bytes

	// Keyboard system control
	#define IGCS_KEY_CAMERA_ENABLE					VK_INSERT
	#define IGCS_KEY_CAMERA_LOCK					VK_SCROLL
	#define IGCS_KEY_ROTATE_RIGHT					VK_RIGHT		// yaw
	#define IGCS_KEY_ROTATE_LEFT					VK_LEFT
	#define IGCS_KEY_ROTATE_UP						VK_UP			// pitch
	#define IGCS_KEY_ROTATE_DOWN					VK_DOWN
	#define IGCS_KEY_MOVE_FORWARD					'W'
	#define IGCS_KEY_MOVE_BACKWARD					'S'
	#define IGCS_KEY_MOVE_LEFT						'A'
	#define IGCS_KEY_MOVE_RIGHT						'D'
	#define IGCS_KEY_MOVE_UP						'R'
	#define IGCS_KEY_MOVE_DOWN						'F'
	#define IGCS_KEY_TILT_LEFT						'Z'		// roll
	#define IGCS_KEY_TILT_RIGHT						'X'
	#define IGCS_KEY_FOV_RESET						VK_HOME
	#define IGCS_KEY_FOV_INCREASE					VK_NEXT
	#define IGCS_KEY_FOV_DECREASE					VK_PRIOR
	#define IGCS_KEY_BLOCK_INPUT					VK_BACK
	#define IGCS_KEY_HUD_TOGGLE						VK_DELETE
	#define IGCS_KEY_TIMESTOP						VK_PAUSE
	#define IGCS_KEY_SKIP_FRAMES					VK_END
	#define IGCS_KEY_DOLLY_ADD_NODE					VK_F5
	#define IGCS_KEY_DOLLY_PLAY_STOP				VK_F6
	#define IGCS_KEY_DOLLY_DELETE_LAST_NODE			VK_F7
	#define IGCS_KEY_DOLLY_CLEAR_PATH				VK_F8
	#define IGCS_KEY_CYCLE_CAMERA_TARGET			VK_F9

	#define IGCS_BUTTON_FOV_DECREASE	Gamepad::button_t::UP
	#define IGCS_BUTTON_FOV_INCREASE	Gamepad::button_t::DOWN
	#define IGCS_BUTTON_RESET_FOV		Gamepad::button_t::B
	#define IGCS_BUTTON_TILT_LEFT		Gamepad::button_t::LEFT
	#define IGCS_BUTTON_TILT_RIGHT		Gamepad::button_t::RIGHT
	#define IGCS_BUTTON_FASTER			Gamepad::button_t::Y
	#define IGCS_BUTTON_SLOWER			Gamepad::button_t::X

	static const uint8_t jmpFarInstructionBytes[6] = { 0xff, 0x25, 0, 0, 0, 0 };	// instruction bytes for jmp qword ptr [0000]

	#define DEVICE_ID_KEYBOARD_MOUSE			0
	#define DEVICE_ID_GAMEPAD					1
	#define DEVICE_ID_ALL						2

	#define IGCS_PIPENAME_DLL_TO_CLIENT				"\\\\.\\pipe\\IgcsDllToClient"
	#define IGCS_PIPENAME_CLIENT_TO_DLL				"\\\\.\\pipe\\IgcsClientToDll"
	

	enum class SettingType : uint8_t
	{
		FastMovementMultiplier = 0,
		SlowMovementMultiplier = 1,
		UpMovementMultiplier = 2,
		MovementSpeed = 3,
		CameraControlDevice = 4,
		RotationSpeed = 5,
		InvertYLookDirection = 6,
		FoVZoomSpeed = 7,
		DollySegmentSeconds = 8,
		DollyLoop = 9,
		
		// add more here
	};

	
	enum class MessageType : uint8_t
	{
		Setting = 1,
		KeyBinding = 2,
		Notification = 3,
		NormalTextMessage = 4,
		ErrorTextMessage = 5,
		DebugTextMessage= 6,
		Action = 7,
	};

	enum class ActionMessageType : uint8_t
	{
		RehookXInput = 1,
	};
}