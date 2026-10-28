////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Byte patterns in this file were derived from Otis_Inf's Elden Ring Camera Tools. PERSONAL USE ONLY, don't publish.
// RVAs in comments are for nightreign.exe, 118,835,320 bytes. Patterns are scanned at runtime, so patches only break
// things if the code around a pattern changes.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "InterceptorHelper.h"
#include "GameConstants.h"
#include "GameImageHooker.h"
#include <map>
#include "MessageHandler.h"
#include "CameraManipulator.h"
#include "Globals.h"

using namespace std;

//--------------------------------------------------------------------------------------------------------------------------------
// external asm functions
extern "C" {
	void cameraStructInterceptor();
}

// external addresses used in asm.
extern "C" {
	LPBYTE _cameraStructInterceptionContinue = nullptr;
}


namespace IGCS::GameSpecific::InterceptorHelper
{
	void initializeAOBBlocks(LPBYTE hostImageAddress, DWORD hostImageSize, map<string, AOBBlock*> &aobBlocks)
	{
		// Camera select/copy routine. RVA 0x1A40C27. In Elden Ring the same code loads the destination with mov rdx,[r9+8],
		// Nightreign uses mov rdx,[r9]. Struct offsets inside the camera are unchanged.
		aobBlocks[CAMERA_ADDRESS_INTERCEPT_KEY] = new AOBBlock(CAMERA_ADDRESS_INTERCEPT_KEY, "8B 41 50 49 8B 11 89 42 50 8B 41 54 89 42 54 8B 41 58 E9", 1);
		// mov rax,[global] ; movss xmm1,[rax+2D4] ; mulss xmm1,[rax+270]. 3 matches, all read the same global. RVA 0xF2E235.
		aobBlocks[TIME_DILATION_LOCATION_KEY] = new AOBBlock(TIME_DILATION_LOCATION_KEY, "48 8B 05 | ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 59 88 ?? ?? ?? ?? 48 8D", 1);
		// CS::CSGraphicsImp, which holds the HUD on/off option at +0x1189. Global RVA 0x3C1A680, 2 matches, both this global.
		aobBlocks[GRAPHICS_MAN_LOCATION_KEY] = new AOBBlock(GRAPHICS_MAN_LOCATION_KEY, "48 8B 05 | ?? ?? ?? ?? 48 85 C0 74 17 48 8B 48 68 48 85 C9", 1);
		// mov rax,[WorldChrMan] ; test rax,rax ; je ; mov rcx,[rax+174E8] (the local player). 29 matches, all the same global. RVA 0x1F058F.
		// PlayAnimation(&hkbCharacterField, L"W_Event80010") from The Grand Archives' ER table (their pattern - 0xD). Unique. RVA 0xD502B0.
		// Nightreign wants the event name as utf-16, ER took ascii.
		aobBlocks[PLAY_ANIMATION_KEY] = new AOBBlock(PLAY_ANIMATION_KEY, "40 53 48 83 EC 40 48 83 39 00 48 8B D9 74 ?? 48 85 D2 74 ?? 48 8D 4C 24 50", 1);
		// Event flags (CS::CSFD4VirtualMemoryFlag, flag id / 1000 = block). Global RVA 0x3C115A8: 2 matches, both this global.
		aobBlocks[EVENT_FLAG_MAN_LOCATION_KEY] = new AOBBlock(EVENT_FLAG_MAN_LOCATION_KEY, "48 8B 3D | ?? ?? ?? ?? 83 F9 FF 41 8B C4 0F 45 C1 89 44 24", 1);
		// bool GetEventFlag(manager, flagId), RVA 0x60CE40. void SetEventFlag(manager, flagId, state), RVA 0x60D330. Both unique.
		aobBlocks[GET_EVENT_FLAG_KEY] = new AOBBlock(GET_EVENT_FLAG_KEY, "44 8B 41 1C 44 8B DA 33 D2 41 8B C3 41 F7 F0", 1);
		aobBlocks[SET_EVENT_FLAG_KEY] = new AOBBlock(SET_EVENT_FLAG_KEY, "48 89 5C 24 08 44 8B 49 1C 44 8B D2 33 D2 41 8B C2 41 F7 F1", 1);
		aobBlocks[WORLD_CHR_MAN_LOCATION_KEY] = new AOBBlock(WORLD_CHR_MAN_LOCATION_KEY, "48 8B 05 | ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 88 E8 74 01 00", 1);
		// 20 byte leaf that copies a 32 byte color (2x 4 floats) for the UI. Otis_Inf's ER tools zero it for their HUD toggle.
		// Zeroing hides every Scaleform UI element (hud, talk boxes, subtitles, menus) at render time, so it works while paused.
		// It misses the weapon / item icons: those go with the game's HUD option. Unique. RVA 0x12C5C60.
		aobBlocks[UI_COLOR_COPY_KEY] = new AOBBlock(UI_COLOR_COPY_KEY, "41 0F 28 00 48 8B C2 41 0F 28 48 ?? 0F 29 02 0F 29 4A ?? C3", 1);

		map<string, AOBBlock*>::iterator it;
		bool result = true;
		for (it = aobBlocks.begin(); it != aobBlocks.end(); it++)
		{
			result &= it->second->scan(hostImageAddress, hostImageSize);
		}
		if (result)
		{
			MessageHandler::logLine("All interception offsets found.");
		}
		else
		{
			MessageHandler::logError("One or more interception offsets weren't found: a game patch may have changed the code. See the log above for which one.");
		}
		if (nullptr == aobBlocks[CAMERA_ADDRESS_INTERCEPT_KEY]->locationInImage())
		{
			MessageHandler::logError("The camera pattern wasn't found, so the camera can't work with this game version.");
		}
	}


	void setCameraStructInterceptorHook(map<string, AOBBlock*> &aobBlocks)
	{
		// We overwrite 15 bytes (5 instructions, our jmp needs 14) and continue at the 6th instruction, mov eax,[rcx+58].
		GameImageHooker::setHook(aobBlocks[CAMERA_ADDRESS_INTERCEPT_KEY], 0x0F, &_cameraStructInterceptionContinue, &cameraStructInterceptor);
	}


	void setPostCameraStructHooks(map<string, AOBBlock*> &aobBlocks)
	{
		AOBBlock* timeDilationBlock = aobBlocks[TIME_DILATION_LOCATION_KEY];
		if (nullptr != timeDilationBlock->locationInImage())
		{
			// the '|' in the pattern marks the rip-relative offset of mov rax,[global]. The next instruction starts 4 bytes later.
			CameraManipulator::setTimeDilationPointerAddress(Utils::calculateAbsoluteAddress(timeDilationBlock, 4));
		}
	}


	// Hides everything: the UI color copy returns zeros (all Scaleform UI, instantly, also while paused), and the game's own
	// "HUD: on/off" option goes off for the weapon / item icons. That option fades with game time, so while paused the icons
	// only follow once the game runs again.
	void toggleHud(map<string, AOBBlock*>& aobBlocks, bool hudVisible)
	{
		AOBBlock* colorBlock = aobBlocks[UI_COLOR_COPY_KEY];
		static uint8_t originalColorCopy[UI_COLOR_COPY_LENGTH];
		static bool originalSaved = false;
		if (nullptr != colorBlock->locationInImage())
		{
			if (!originalSaved)
			{
				memcpy(originalColorCopy, colorBlock->locationInImage(), UI_COLOR_COPY_LENGTH);
				originalSaved = true;
			}
			// xorps xmm0,xmm0 ; mov rax,rdx ; movaps [rdx],xmm0 ; movaps [rdx+10h],xmm0 ; ret ; nops
			uint8_t zeroColor[UI_COLOR_COPY_LENGTH] = { 0x0F, 0x57, 0xC0, 0x48, 0x8B, 0xC2, 0x0F, 0x29, 0x02, 0x0F, 0x29, 0x42, 0x10, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
			GameImageHooker::writeRange(colorBlock->locationInImage(), hudVisible ? originalColorCopy : zeroColor, UI_COLOR_COPY_LENGTH);
		}
		AOBBlock* graphicsBlock = aobBlocks[GRAPHICS_MAN_LOCATION_KEY];
		LPBYTE graphics = nullptr == graphicsBlock->locationInImage() ? nullptr : *reinterpret_cast<LPBYTE*>(Utils::calculateAbsoluteAddress(graphicsBlock, 4));
		if (nullptr != graphics)
		{
			graphics[HUD_OPTION_OFFSET] = hudVisible ? 1 : 0;
		}
		else if (nullptr == colorBlock->locationInImage())
		{
			MessageHandler::addNotification("HUD toggle isn't available right now");
		}
	}
}
