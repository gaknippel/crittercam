//////////////////////////////////////////////////////////////////////
// By Adam Dobos: https://github.com/adam10603/XInput-wrapper
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "Gamepad.h"
#include <Windows.h>
#include <Xinput.h>
#include <algorithm>
#include <limits>
#include <cmath>

#define clamp(v, _min, _max) max(min(v, _max), _min)

XINPUT_STATE* Gamepad::getState() { return &gpState; }
int Gamepad::getIndex() { return gpIndex; }
bool Gamepad::isConnected() { return connected; }
void Gamepad::setButtonDownCallback(function<void(button_t)> fn) { buttonDownCallback = fn; }
void Gamepad::setButtonUpCallback(function<void(button_t)> fn) { buttonUpCallback = fn; }
bool Gamepad::isButtonPressed(button_t b) { return (buttonState & b); }
void Gamepad::setInvertLStickY(bool b) { invertLSY = b; }
void Gamepad::setInvertRStickY(bool b) { invertRSY = b; }

// Read through the game's own xinput1_4: Steam Input hooks the XInput DLLs that are loaded when the game starts, so its virtual pad
// (PlayStation pads etc.) doesn't show up through the xinput9_1_0 we link against, which only loads when we're injected.
typedef DWORD(WINAPI* XINPUTGETSTATE)(DWORD, XINPUT_STATE*);
static XINPUTGETSTATE resolveXInputGetState() {
	HMODULE xinput = GetModuleHandleW(L"xinput1_4.dll");
	XINPUTGETSTATE fn = xinput ? reinterpret_cast<XINPUTGETSTATE>(GetProcAddress(xinput, "XInputGetState")) : nullptr;
	return fn ? fn : &XInputGetState;
}

void Gamepad::update() {
	static const XINPUTGETSTATE getState = resolveXInputGetState();
	ZeroMemory(&gpState, sizeof(XINPUT_STATE));
	connected = (getState(gpIndex, &gpState) == ERROR_SUCCESS);
	if (!connected) {
		gpIndex = (gpIndex + 1) % XUSER_MAX_COUNT;		// Steam's virtual pad isn't always slot 0: try the next slot on the next update
	}
	if (buttonState != gpState.Gamepad.wButtons) {
		WORD stateDiff = buttonState ^ gpState.Gamepad.wButtons;
		WORD buttonStateCopy = buttonState;
		for (WORD i{ 0x1 };; i <<= 0x1) {
			if (stateDiff & 0x1) {
				if (buttonStateCopy & 0x1) {
					if (buttonUpCallback)
						buttonUpCallback(static_cast<button_t>(i));
				} else {
					if (buttonDownCallback)
						buttonDownCallback(static_cast<button_t>(i));
				}
			}
			if (i == 0x8000) break;
			else {
				stateDiff >>= 0x1;
				buttonStateCopy >>= 0x1;
			}
		}
	}
	buttonState = gpState.Gamepad.wButtons;
}

// Radial deadzone rescaled to 0..1, then a squared response curve: precise near the center, full speed at the edge (like most games).
static vec2 shapeStick(SHORT rawX, SHORT rawY, bool invertY, float deadzone) {
	float x = static_cast<float>(rawX) / static_cast<float>(SHRT_MAX);
	float y = (invertY ? -1.0f : 1.0f) * static_cast<float>(rawY) / static_cast<float>(SHRT_MAX);
	float length = sqrtf(x * x + y * y);
	if (length <= deadzone) {
		return { 0.0f, 0.0f };
	}
	float scaled = (std::min)((length - deadzone) / (1.0f - deadzone), 1.0f);
	float factor = scaled * scaled / length;
	return { x * factor, y * factor };
}

vec2 Gamepad::getLStickPosition() {
	return shapeStick(gpState.Gamepad.sThumbLX, gpState.Gamepad.sThumbLY, invertLSY, static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / SHRT_MAX);
}

vec2 Gamepad::getRStickPosition() {
	return shapeStick(gpState.Gamepad.sThumbRX, gpState.Gamepad.sThumbRY, invertRSY, static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / SHRT_MAX);
}

float Gamepad::getLTrigger() {
	auto x = gpState.Gamepad.bLeftTrigger;
	return (x > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? static_cast<float>(x) / 255.0f : 0.0f;
}

float Gamepad::getRTrigger() {
	auto x = gpState.Gamepad.bRightTrigger;
	return (x > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ? static_cast<float>(x) / 255.0f : 0.0f;
}

void Gamepad::vibrate(float L, float R) {
	L = clamp(L, 0.0f, 1.0f);
	R = clamp(R, 0.0f, 1.0f);
	XINPUT_VIBRATION vState;
	ZeroMemory(&vState, sizeof(XINPUT_VIBRATION));
	int iL = static_cast<WORD>(L * 65535.0f + 0.4999f);
	int iR = static_cast<WORD>(R * 65535.0f + 0.4999f);
	vState.wLeftMotorSpeed = iL;
	vState.wRightMotorSpeed = iR;
	XInputSetState(gpIndex, &vState);
}
