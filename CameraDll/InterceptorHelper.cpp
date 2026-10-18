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
	void hudOpacityReadInterceptor();
}

// external addresses used in asm.
extern "C" {
	LPBYTE _cameraStructInterceptionContinue = nullptr;
	LPBYTE _hudOpacityReadInterceptionContinue = nullptr;		// unused: the HUD interceptor returns to the caller itself.
	uint8_t g_hudHidden = 0;
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
		// 32 byte copy function used by Otis_Inf's HUD toggle in Elden Ring. Identical in Nightreign. RVA 0x12C5C60.
		aobBlocks[HUD_OPACITY_READ_INTERCEPT_KEY] = new AOBBlock(HUD_OPACITY_READ_INTERCEPT_KEY, "41 0F 28 00 48 8B C2 41 0F 28 48 ?? 0F 29 02 0F 29 4A", 1);
		// mov rax,[WorldChrMan] ; test rax,rax ; je ; mov rcx,[rax+174E8] (the local player). 29 matches, all the same global. RVA 0x1F058F.
		aobBlocks[WORLD_CHR_MAN_LOCATION_KEY] = new AOBBlock(WORLD_CHR_MAN_LOCATION_KEY, "48 8B 05 | ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 88 E8 74 01 00", 1);

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
		// The HUD interceptor replaces the whole (20 byte) function and returns to the caller itself.
		GameImageHooker::setHook(aobBlocks[HUD_OPACITY_READ_INTERCEPT_KEY], 0x14, &_hudOpacityReadInterceptionContinue, &hudOpacityReadInterceptor);

		AOBBlock* timeDilationBlock = aobBlocks[TIME_DILATION_LOCATION_KEY];
		if (nullptr != timeDilationBlock->locationInImage())
		{
			// the '|' in the pattern marks the rip-relative offset of mov rax,[global]. The next instruction starts 4 bytes later.
			CameraManipulator::setTimeDilationPointerAddress(Utils::calculateAbsoluteAddress(timeDilationBlock, 4));
		}
	}


	void toggleHud(map<string, AOBBlock*>& aobBlocks, bool hudVisible)
	{
		if (nullptr == aobBlocks[HUD_OPACITY_READ_INTERCEPT_KEY]->locationInImage())
		{
			MessageHandler::addNotification("HUD toggle isn't available for this game version");
			return;
		}
		g_hudHidden = hudVisible ? (uint8_t)0 : (uint8_t)1;
	}
}
