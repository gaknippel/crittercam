// NightreignAOBs.h
// Byte patterns for a Nightreign camera tool, ported from Otis_Inf's Elden Ring Camera Tools v1.0.18.
// Scanned against: nightreign.exe (118,835,320 bytes) and eldenring.exe (87,042,128 bytes) from E:\SteamLibrary.
// Every pattern below matches exactly ONE place in nightreign.exe (time dilation: 3 places, all the same global).
// RVAs are for this exe build only - always find them by pattern at runtime; game patches move them.
// Personal use only. Patterns derived from Otis's closed-source tool - don't publish.
#pragma once

namespace NightreignAOB
{
    // ---------------------------------------------------------------------------------
    // CAMERA  (ported - code changed, struct layout did not)
    // Hook point: the routine that copies the camera matrix + FoV into the render camera.
    // At the hook, RCX = source camera, RDX = destination camera.
    //   Camera struct (same as ER):
    //     +0x10 .. +0x4F  4x4 matrix (rows 0-2 = rotation, row 3 (+0x40) = position)
    //     +0x50  FoV (float)     +0x54  aspect ratio     +0x58  near plane     +0x5C  far plane
    // Differences from ER:
    //   - ER loads dest with  mov rdx,[r9+8]   -> NR uses  mov rdx,[r9]
    //   - The parent object's pointer slots moved down 8 bytes (+0x10/+0x18/+0x20 -> +0x08/+0x10/+0x18)
    //   - NR's protection layer splits this function: the copy ends in a JMP to another chunk,
    //     so hook BEFORE that jmp. 6 bytes of real instructions are available (enough for a 5-byte jmp).
    // NR RVA: 0x1A40C27
    constexpr const char* CAMERA_ADDRESS_INTERCEPT = "8B 41 50 49 8B 11 89 42 50 8B 41 54 89 42 54 8B 41 58 E9";

    // ---------------------------------------------------------------------------------
    // PAUSE (real engine pause - Otis's "menu explanation pause")  (ported - jump distance changed)
    // Gate tests bit 18 of a flags dword, then sets a global pause byte = 1.
    // Differences from ER: flags dword at +0x48 (ER +0x1C), second object's byte at +0x138 (ER +0x140).
    // NR RVA: 0x566F31
    constexpr const char* PAUSE_ENGINE_ADDRESS_INTERCEPT = "C1 E8 12 A8 01 74 ?? 48 8B 0D ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 48 85 C9 74 05";

    // ---------------------------------------------------------------------------------
    // TIME DILATION / GAME SPEED  (Otis's ER pattern matches unchanged)
    // '|' marks where the rip-relative offset starts (resolve it to get the global pointer).
    // Global pointer: NR RVA 0x442FEF8 (ER 0x458DB58).
    // Fields multiplied together: NR +0x2D4 and +0x270 (ER +0x2CC and +0x268) - both shifted +8.
    // Setting the speed field to 0 = timestop; 0.25 = slow-mo. Verify which field is speed in game.
    // NR RVAs: 0xF2E235, 0xF2F2E5, 0xF2F7A7
    constexpr const char* TIME_DILATION_LOCATION = "48 8B 05 | ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 59 88 ?? ?? ?? ?? 48 8D";
    constexpr int TIME_DILATION_SPEED_OFFSET  = 0x2D4;   // verify
    constexpr int TIME_DILATION_FACTOR_OFFSET = 0x270;   // verify

    // ---------------------------------------------------------------------------------
    // HUD OPACITY  (matches unchanged; surrounding code is identical to ER)
    // Copies a 32-byte color/opacity value from [r8] to [rdx]. Hook and zero the alpha to hide the HUD.
    // NR RVA: 0x12C5C60
    constexpr const char* HUD_OPACITY_READ = "41 0F 28 00 48 8B C2 41 0F 28 48 ?? 0F 29 02 0F 29 4A";

    // ---------------------------------------------------------------------------------
    // INVISIBILITY TO ENEMIES  (matches unchanged; one nearby field moved 0xC980 -> 0xC970)
    // NR RVA: 0x276DA9
    constexpr const char* INVISIBILITY = "74 0B 48 8B 01 F3 0F 10 4E ?? FF 50 ?? 48 8D 8F";

    // ---------------------------------------------------------------------------------
    // NOT PORTED YET (no match in NR; not needed for free cam / dolly / pause)
    //   PLAYER_COORDINATES_INTERCEPT   - needed only for player-relative camera paths
    //   RESOLUTION_STRUCT_LOCATION     - hotsampling
    //   VIGNETTE_DATA_READ, CA_ENABLE_BYTE_WRITE, DITHER_VALUE_READ, unnamed IQ pattern
    //   TOD_WRITE_INTERCEPT            - sun direction
    //   CAMERA_ADDRESS_INTERCEPT alt #2 (ER RVA 0x465C4F, camera position calc using +0x180/+0x184/+0x188)
}
