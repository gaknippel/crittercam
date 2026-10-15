////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include <map>
#include "Utils.h"

namespace IGCS::GameSpecific::InterceptorHelper
{
	void initializeAOBBlocks(LPBYTE hostImageAddress, DWORD hostImageSize, std::map<std::string, AOBBlock*> &aobBlocks);
	void setCameraStructInterceptorHook(std::map<std::string, AOBBlock*> &aobBlocks);
	void setPostCameraStructHooks(std::map<std::string, AOBBlock*> &aobBlocks);
	void toggleHud(std::map<std::string, AOBBlock*>& aobBlocks, bool hudVisible);
}
