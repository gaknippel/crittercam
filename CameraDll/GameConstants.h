////////////////////////////////////////////////////////////////////////////////////////////////////////
// crittercam - camera tools for ELDEN RING NIGHTREIGN
// Built on Injectable Generic Camera System, Copyright(c) 2020, Frans Bouma (BSD-2, see LICENSE-IGCS.txt)
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Byte patterns used with these keys were derived from Otis_Inf's Elden Ring Camera Tools. PERSONAL USE ONLY.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace IGCS::GameSpecific
{
	// Mandatory constants to define for a game
	#define GAME_NAME									"ELDEN RING NIGHTREIGN"
	#define CAMERA_VERSION								"0.1.0"
	#define CAMERA_CREDITS								"crittercam. Built on IGCS by Otis_Inf."
	#define GAME_WINDOW_TITLE							"ELDEN RING"	// 'starts with' match on the game window title
	#define INITIAL_PITCH_RADIANS						0.0f
	#define INITIAL_YAW_RADIANS							0.0f
	#define INITIAL_ROLL_RADIANS						0.0f
	#define CONTROLLER_Y_INVERT							false
	// These will be overwritten by settings sent by the client. These defines are for initial usage.
	#define FASTER_MULTIPLIER							5.0f
	#define SLOWER_MULTIPLIER							0.1f
	#define MOUSE_SPEED_CORRECTION						0.2f	// to correct for the mouse-deltas related to normal rotation.
	#define DEFAULT_MOVEMENT_SPEED						0.05f	// world units (meters) per tick
	#define DEFAULT_ROTATION_SPEED						0.01f	// radians per tick
	#define DEFAULT_FOV_SPEED							0.3f	// 'degrees' per tick, scaled by FOV_CHANGE_SCALE below
	#define DEFAULT_UP_MOVEMENT_MULTIPLIER				0.7f
	#define DEFAULT_FOV									0.84f	// radians. Only used as a fallback before the game fov is known.
	#define DEFAULT_DOLLY_SEGMENT_SECONDS				3.0f
	#define DOLLY_SEGMENT_SECONDS_MIN					0.1f
	#define DOLLY_SEGMENT_SECONDS_MAX					60.0f
	// End Mandatory constants

	// The game stores the FoV in radians. The client's FoV speed setting is in 'degree-ish' units, this scales it down.
	#define FOV_CHANGE_SCALE							0.01f
	#define FOV_MIN										0.01f
	#define FOV_MAX										3.0f
	#define GAME_SPEED_MIN								0.01f
	#define GAME_SPEED_MAX								4.0f
	#define GAME_SPEED_WHEEL_STEP						1.15f	// shift + one wheel notch multiplies/divides the game speed by this
	#define GAME_SPEED_WHEEL_MIN						0.05f	// same lower end as the client's slider
	#define GAME_SPEED_EASE_SECONDS						0.25f	// how fast the game eases to a new speed (0 = instant)
	#define PITCH_LIMIT									1.55f	// radians (~89 degrees)

	// AOB Keys for interceptor's AOB scanner
	#define CAMERA_ADDRESS_INTERCEPT_KEY				"AOB_CAMERA_ADDRESS_INTERCEPT"
	#define TIME_DILATION_LOCATION_KEY					"AOB_TIME_DILATION_LOCATION"
	#define HUD_OPACITY_READ_INTERCEPT_KEY				"AOB_HUD_OPACITY_READ_INTERCEPT"
	#define WORLD_CHR_MAN_LOCATION_KEY					"AOB_WORLD_CHR_MAN_LOCATION"

	// Characters (found with RTTI on 2026-10-17, see docs/NightreignAOBs.h). WorldChrMan is CS::WorldChrManImp.
	#define WORLD_CHR_MAN_MAIN_PLAYER_OFFSET			0x174E8		// PlayerIns* of the local player
	#define WORLD_CHR_MAN_CHARACTERS_OFFSET				0x17E60		// ChrIns* array: players, then enemies. Ends at the first non-character
	#define MAX_CHARACTERS								256
	#define CHR_MODULES_OFFSET							0x1B8		// ChrIns -> module bag
	#define CHR_DATA_MODULE_OFFSET						0x0			// bag -> CSChrDataModule
	#define CHR_PHYSICS_MODULE_OFFSET					0x68		// bag -> CSChrPhysicsModule
	#define CHR_HP_OFFSET								0x140		// CSChrDataModule -> current hp (int)
	#define CHR_POSITION_OFFSET							0x70		// CSChrPhysicsModule -> x, y, z (floats)

	// Camera struct layout. Same as Elden Ring. The game copies a source camera into the render camera:
	//   +0x10 .. +0x4F  4x4 matrix: row 0 = right, row 1 = up, row 2 = forward, row 3 = position (w = 1)
	//   +0x50 FoV (radians)   +0x54 aspect ratio   +0x58 near plane   +0x5C far plane
	#define CAMERA_MATRIX_IN_STRUCT_OFFSET				0x10
	#define CAMERA_POSITION_IN_STRUCT_OFFSET			0x40
	#define FOV_IN_STRUCT_OFFSET						0x50
	#define CAMERA_STRUCT_SIZE							0x60

	// Time dilation: a global pointer (found via TIME_DILATION_LOCATION) to a struct with two floats that the game multiplies
	// together each frame. One of them is the game speed (1.0), the other one the frame delta. Which one is which isn't verified yet,
	// so the code picks the one that's 1.0 at the time you pause. Elden Ring used +0x2CC/+0x268; Nightreign shifted both by 8.
	#define TIME_DILATION_FLOAT1_OFFSET					0x2D4
	#define TIME_DILATION_FLOAT2_OFFSET					0x270

	// Max number of distinct destination cameras the camera copy routine is seen writing to. Must match the asm loop limit.
	#define MAX_CAMERA_CANDIDATES						8
}
