////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Studio: recording your own character. Play any behavior event (gestures), float without gravity, nudge up / down.
// Found live on 2026-10-17, see docs/NightreignAOBs.h. Every pointer walk is guarded: the player is gone during loading screens.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "Studio.h"
#include "GameConstants.h"
#include "MessageHandler.h"
#include "Utils.h"
#include <atomic>

namespace IGCS::Studio
{
	// PlayAnimation(&hkbCharacterField, eventName): 0 = played, -1 = the character has no such event
	typedef int(__fastcall* PLAY_ANIMATION)(uintptr_t* hkbCharacterField, const wchar_t* eventName);

	static LPBYTE s_worldChrManPointer = nullptr;
	static PLAY_ANIMATION s_playAnimation = nullptr;
	static std::atomic_bool s_floating = false;
	static std::atomic_bool s_invincible = false;
	static std::atomic_bool s_enemiesPassive = false;
	static const wchar_t* IDLE_EVENT = L"W_Idle";
	static const uint8_t NO_DEATH_BIT = 0b01;		// ER's no damage bit (0b10) does nothing in Nightreign, so hp is refilled instead


	typedef bool(__fastcall* GET_EVENT_FLAG)(uintptr_t manager, uint32_t flagId);
	typedef void(__fastcall* SET_EVENT_FLAG)(uintptr_t manager, uint32_t flagId, bool state);
	static LPBYTE s_eventFlagManPointer = nullptr;
	static GET_EVENT_FLAG s_getEventFlag = nullptr;
	static SET_EVENT_FLAG s_setEventFlag = nullptr;


	void initializeEventFlags(LPBYTE managerPointer, LPBYTE getEventFlag, LPBYTE setEventFlag)
	{
		s_eventFlagManPointer = managerPointer;
		s_getEventFlag = reinterpret_cast<GET_EVENT_FLAG>(getEventFlag);
		s_setEventFlag = reinterpret_cast<SET_EVENT_FLAG>(setEventFlag);
	}


	// -1 = not available (pattern not found or no manager yet), else 0 / 1
	static int callEventFlag(uint32_t flagId, int newState)
	{
		__try
		{
			const uintptr_t manager = nullptr == s_eventFlagManPointer ? 0 : *reinterpret_cast<uintptr_t*>(s_eventFlagManPointer);
			if (0 == manager || nullptr == s_getEventFlag || nullptr == s_setEventFlag)
			{
				return -1;
			}
			if (newState >= 0)
			{
				s_setEventFlag(manager, flagId, 1 == newState);
			}
			return s_getEventFlag(manager, flagId) ? 1 : 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return -1;
		}
	}


	static void notifyFlag(uint32_t flagId, int state)
	{
		MessageHandler::addNotification(state < 0 ? std::string("Event flags aren't available right now")
												  : Utils::formatString("Event flag %u is %s", flagId, 1 == state ? "on" : "off"));
	}


	void setEventFlag(uint32_t flagId, bool on)
	{
		notifyFlag(flagId, callEventFlag(flagId, on ? 1 : 0));
	}


	void reportEventFlag(uint32_t flagId)
	{
		notifyFlag(flagId, callEventFlag(flagId, -1));
	}


	void initialize(LPBYTE worldChrManPointer, LPBYTE playAnimation)
	{
		s_worldChrManPointer = worldChrManPointer;
		s_playAnimation = reinterpret_cast<PLAY_ANIMATION>(playAnimation);
	}


	// the local player's ChrIns, 0 when there is none
	static uintptr_t localPlayer()
	{
		__try
		{
			const uintptr_t worldChrMan = nullptr == s_worldChrManPointer ? 0 : *reinterpret_cast<uintptr_t*>(s_worldChrManPointer);
			return 0 == worldChrMan ? 0 : *reinterpret_cast<uintptr_t*>(worldChrMan + WORLD_CHR_MAN_MAIN_PLAYER_OFFSET);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return 0;
		}
	}


	static uintptr_t playerModule(int offset)
	{
		__try
		{
			const uintptr_t player = localPlayer();
			return 0 == player ? 0 : *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(player + CHR_MODULES_OFFSET) + offset);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return 0;
		}
	}


	static int callPlayAnimation(const wchar_t* eventName)
	{
		__try
		{
			const uintptr_t behavior = playerModule(CHR_BEHAVIOR_MODULE_OFFSET);
			if (0 == behavior || nullptr == s_playAnimation)
			{
				return -2;
			}
			const uintptr_t holder = *reinterpret_cast<uintptr_t*>(behavior + CHR_BEHAVIOR_HKB_HOLDER_OFFSET);
			return s_playAnimation(reinterpret_cast<uintptr_t*>(holder + HKB_CHARACTER_FIELD_OFFSET), eventName);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return -3;
		}
	}


	// Play once: the game loops gesture clips forever (its own gesture code stops them, which we skip). update() watches the
	// clip W_EventN started (animation id N) and goes back to idle when it ends. A follow-up clip N+1 (the seated loop of
	// sitting gestures) is left playing until stop / the next gesture.
	static std::atomic_int s_onceAnimation = 0;
	static std::atomic<ULONGLONG> s_onceStartedAt = 0;
	static bool s_onceSeen = false;
	static float s_onceLastTime = 0.0f;
	static const ULONGLONG ONCE_START_TIMEOUT_MS = 1000;		// the clip never showed up (not a W_EventN clip): stop watching
	static const ULONGLONG ONCE_GRACE_MS = 200;				// replaying the clip that's playing shows its old time for a frame, which looks like a wrap

	// the clip the player is playing now; false when there's no player
	static bool currentClip(int& animationId, float& time, float& length)
	{
		__try
		{
			const uintptr_t timeAct = playerModule(CHR_TIME_ACT_MODULE_OFFSET);
			if (0 == timeAct)
			{
				return false;
			}
			animationId = *reinterpret_cast<int*>(timeAct + TIME_ACT_ANIMATION_ID_OFFSET);
			time = *reinterpret_cast<float*>(timeAct + TIME_ACT_ANIMATION_TIME_OFFSET);
			length = *reinterpret_cast<float*>(timeAct + TIME_ACT_ANIMATION_LENGTH_OFFSET);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	static void updatePlayOnce()
	{
		const int target = s_onceAnimation;
		int animationId = 0;
		float time = 0.0f, length = 0.0f;
		const ULONGLONG elapsed = GetTickCount64() - s_onceStartedAt;
		if (0 == target || elapsed < ONCE_GRACE_MS || !currentClip(animationId, time, length))
		{
			return;
		}
		if (animationId == target)
		{
			const bool ended = (s_onceSeen && time < s_onceLastTime) || time >= length;		// looping clips wrap, one-shot clips stop at their length
			s_onceSeen = true;
			s_onceLastTime = time;
			if (ended)
			{
				s_onceAnimation = 0;
				callPlayAnimation(IDLE_EVENT);
			}
			return;
		}
		// moved on: to its seated loop, to something the player did, or the clip never started
		if (s_onceSeen || elapsed > ONCE_START_TIMEOUT_MS)
		{
			s_onceAnimation = 0;
		}
	}


	void playAnimation(const std::string& eventName)
	{
		std::wstring wide(MultiByteToWideChar(CP_UTF8, 0, eventName.c_str(), static_cast<int>(eventName.size()), nullptr, 0), L' ');
		MultiByteToWideChar(CP_UTF8, 0, eventName.c_str(), static_cast<int>(eventName.size()), wide.data(), static_cast<int>(wide.size()));
		s_onceAnimation = 0;
		const int result = callPlayAnimation(wide.c_str());
		if (0 != result)
		{
			MessageHandler::addNotification(-1 == result ? Utils::formatString("No animation called %s", eventName.c_str())
														 : std::string("Can't play animations right now (no character, or this game version isn't supported)"));
			return;
		}
		int animationId = 0;
		if (1 == swscanf_s(wide.c_str(), L"W_Event%d", &animationId))
		{
			s_onceSeen = false;
			s_onceStartedAt = GetTickCount64();
			s_onceAnimation = animationId;
		}
	}


	static bool writePhysicsByte(int offset, uint8_t value)
	{
		__try
		{
			const uintptr_t physics = playerModule(CHR_PHYSICS_MODULE_OFFSET);
			if (0 == physics)
			{
				return false;
			}
			*reinterpret_cast<uint8_t*>(physics + offset) = value;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	static bool addHeight(float meters)
	{
		__try
		{
			const uintptr_t physics = playerModule(CHR_PHYSICS_MODULE_OFFSET);
			if (0 == physics)
			{
				return false;
			}
			*reinterpret_cast<float*>(physics + CHR_POSITION_OFFSET + 4) += meters;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}


	static void setDebugFlags(bool on)
	{
		__try
		{
			const uintptr_t data = playerModule(CHR_DATA_MODULE_OFFSET);
			if (0 == data)
			{
				return;
			}
			uint8_t* flags = reinterpret_cast<uint8_t*>(data + CHR_DEBUG_FLAGS_OFFSET);
			*flags = on ? (*flags | NO_DEATH_BIT) : (*flags & ~NO_DEATH_BIT);
			if (on)
			{
				*reinterpret_cast<int*>(data + CHR_HP_OFFSET) = *reinterpret_cast<int*>(data + CHR_MAX_HP_OFFSET);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}


	// Enemies ignore you: every enemy is kept on the player's team, so it doesn't target you (it stays alive and moving, and its
	// hits do nothing). Their original teams are remembered so turning it off puts them back. Plain arrays: used inside __try.
	static uintptr_t s_passiveChr[MAX_CHARACTERS];
	static uint8_t s_passiveTeam[MAX_CHARACTERS];
	static int s_passiveCount = 0;

	// restore = false: move enemies to the player's team (remembering their team). restore = true: give remembered teams back.
	static void updateEnemyTeams(bool restore)
	{
		__try
		{
			const uintptr_t worldChrMan = nullptr == s_worldChrManPointer ? 0 : *reinterpret_cast<uintptr_t*>(s_worldChrManPointer);
			if (0 == worldChrMan)
			{
				return;
			}
			const uintptr_t player = *reinterpret_cast<uintptr_t*>(worldChrMan + WORLD_CHR_MAN_MAIN_PLAYER_OFFSET);
			uintptr_t* begin = *reinterpret_cast<uintptr_t**>(worldChrMan + WORLD_CHR_MAN_CHARACTERS_BEGIN_OFFSET);
			uintptr_t* end = *reinterpret_cast<uintptr_t**>(worldChrMan + WORLD_CHR_MAN_CHARACTERS_END_OFFSET);
			if (0 == player || nullptr == begin || end < begin || end - begin > MAX_CHARACTERS)
			{
				return;
			}
			const uintptr_t playerVtable = *reinterpret_cast<uintptr_t*>(player);
			const uint8_t playerTeam = *reinterpret_cast<uint8_t*>(player + CHR_TEAM_TYPE_OFFSET);
			for (uintptr_t* it = begin; it < end; it++)
			{
				const uintptr_t chr = *it;
				if (0 == chr || chr == player || *reinterpret_cast<uintptr_t*>(chr) == playerVtable)
				{
					continue;		// you and other players
				}
				uint8_t* team = reinterpret_cast<uint8_t*>(chr + CHR_TEAM_TYPE_OFFSET);
				int saved = 0;
				while (saved < s_passiveCount && s_passiveChr[saved] != chr)
				{
					saved++;
				}
				if (restore)
				{
					if (saved < s_passiveCount)
					{
						*team = s_passiveTeam[saved];		// only characters still in the live list, never a stale address
					}
					continue;
				}
				if (TEAM_TYPE_PLAYER_ATTACHMENT == *team || playerTeam == *team)
				{
					continue;
				}
				if (saved == s_passiveCount && s_passiveCount < MAX_CHARACTERS)
				{
					s_passiveChr[s_passiveCount] = chr;
					s_passiveTeam[s_passiveCount] = *team;
					s_passiveCount++;
				}
				*team = playerTeam;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		if (restore)
		{
			s_passiveCount = 0;
		}
	}


	void setEnemiesPassive(bool passive)
	{
		s_enemiesPassive = passive;		// applied / undone by update() on the main loop, so only one thread touches the arrays
		MessageHandler::addNotification(passive ? "Enemies ignore you" : "Enemies back to normal");
	}


	void setInvincible(bool invincible)
	{
		s_invincible = invincible;
		setDebugFlags(invincible);
		MessageHandler::addNotification(invincible ? "Invincible" : "Invincible off");
	}


	void setFloating(bool floating)
	{
		s_floating = floating;
		writePhysicsByte(CHR_NO_GRAVITY_OFFSET, floating ? 1 : 0);
		MessageHandler::addNotification(floating ? "Floating" : "Floating off");
	}


	void lift(float meters)
	{
		if (!s_floating)
		{
			setFloating(true);
		}
		addHeight(meters);
	}


	void update()
	{
		if (s_floating)
		{
			writePhysicsByte(CHR_NO_GRAVITY_OFFSET, 1);
		}
		if (s_enemiesPassive)
		{
			updateEnemyTeams(false);		// enemies that spawn or load in later are included
		}
		else if (s_passiveCount > 0)
		{
			updateEnemyTeams(true);
		}
		if (s_invincible)
		{
			setDebugFlags(true);		// the game rebuilds the player on loading screens, which clears it
		}
		updatePlayOnce();
	}
}
