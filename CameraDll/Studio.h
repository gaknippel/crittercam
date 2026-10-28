////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Studio: recording your own character. Play any behavior event (gestures), float without gravity, nudge up / down.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include <string>

namespace IGCS::Studio
{
	// worldChrManPointer: address of the game's WorldChrMan global. playAnimation: the game's PlayAnimation function. Either may be null.
	void initialize(LPBYTE worldChrManPointer, LPBYTE playAnimation);
	// eventName is a behavior event like "W_Event80010" (utf-8, the game wants it as utf-16)
	void playAnimation(const std::string& eventName);
	void setFloating(bool floating);
	// enemies don't target you (kept on your team): alive and moving, their hits do nothing. Off restores their teams
	void setEnemiesPassive(bool passive);
	// no death + no damage for the local player
	void setInvincible(bool invincible);
	// moves the player up (or down) and turns floating on, otherwise they'd just fall back
	void lift(float meters);
	// event flags: the game's own GetEventFlag / SetEventFlag. Reports the result as a notification
	void initializeEventFlags(LPBYTE managerPointer, LPBYTE getEventFlag, LPBYTE setEventFlag);
	void setEventFlag(uint32_t flagId, bool on);
	void reportEventFlag(uint32_t flagId);
	// turns the game's own (hidden) follow camera to this yaw, so the game moves the character relative to the free cam
	void alignGameCameraYaw(float yaw);
	// the local player's feet, false when there's no player (loading screen)
	bool localPlayerPosition(float* xyz);
	// every frame: keeps the float flag on, the game can reset it
	void update();
}
