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
    // HUD (2026-10-18): the game's own HUD option, CS::CSGraphicsImp +0x1189 (1 on, 0 off). Found by diffing memory while toggling
    // Settings > HUD. Replaces the old HUD_OPACITY_READ hook (Otis's ER guess), which missed the item/skill icons.
    //   That option fades over ~0.05-0.2s of GAME time, so while paused (speed 0.0001) it never finishes, and it doesn't hide
    //   talk boxes / subtitles. (2026-10-27) Delete now does both: zero the UI color copy at RVA 0x12C5C60 (all Scaleform UI,
    //   render time, works paused; patched in place, no hook) + the option (the weapon / item icons, which ignore the color copy).
    //   Tried and ruled out for the icons: the sibling copy at 0x12C5C40, CSFeManImp fields, CSFeAutoHideCtrl (static table only).
    // Global RVA 0x3C1A680, 2 matches, both this global.
    constexpr const char* GRAPHICS_MAN = "48 8B 05 | ?? ?? ?? ?? 48 85 C0 74 17 48 8B 48 68 48 85 C9";
    // PAUSE: speed 0 makes the game's own camera fly off (it divides by the frame time). Pause uses 0.0001 instead.

    // ---------------------------------------------------------------------------------
    // INVISIBILITY TO ENEMIES  (matches unchanged; one nearby field moved 0xC980 -> 0xC970)
    // NR RVA: 0x276DA9
    constexpr const char* INVISIBILITY = "74 0B 48 8B 01 F3 0F 10 4E ?? FF 50 ?? 48 8D 8F";

    // ---------------------------------------------------------------------------------
    // WORLD CHR MAN  (found 2026-10-17 via RTTI: .?AVWorldChrManImp@CS@@, .?AVPlayerIns@CS@@, .?AVEnemyIns@CS@@; ER patterns don't match)
    // mov rax,[global] ; test rax,rax ; je ; mov rcx,[rax+174E8]. 29 matches, all the same global (RVA 0x3C0F0A8). NR RVA: 0x1F058F
    //   WorldChrMan +0x174E8 local PlayerIns*, +0x17E60 ChrIns* array (players then enemies, ends at the first non-character)
    //   ChrIns +0x1B8 module bag: +0x0 CSChrDataModule (hp int at +0x140), +0x68 CSChrPhysicsModule (quat +0x60, position +0x70)
    constexpr const char* WORLD_CHR_MAN = "48 8B 05 | ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 88 E8 74 01 00";

    // ---------------------------------------------------------------------------------
    // PLAY ANIMATION  (The Grand Archives ER table: AOB "74 ?? 48 85 d2 74 ?? 48 8d 4c 24 50" - 0xD, unique in NR too)
    // int PlayAnimation(hkbCharacter** field, const wchar_t* behaviorEvent). NR takes UTF-16 (ER: ascii). 0 = ok, -1 = unknown event.
    // field = [[ChrIns+0x1B8]+0x28 (CSChrBehaviorModule)]+0x10, then +0x30 (that slot holds the hkbCharacter*).
    // Gestures are W_Event80000..W_Event81370 (78 found in memory). Called from a non-game thread, like CE's executeCodeEx.
    // NR RVA: 0xD502B0
    constexpr const char* PLAY_ANIMATION = "40 53 48 83 EC 40 48 83 39 00 48 8B D9 74 ?? 48 85 D2 74 ?? 48 8D 4C 24 50";
    // CURRENT CLIP: bag +0x18 CSChrTimeActModule: +0x34 int animation id (W_Event80070 plays 80070), +0x3C float time,
    //   +0x40 float length (wave 5.67s). Gesture clips loop (time wraps to 0); one-shot clips (prayer) stop at length;
    //   sitting gestures move on to id+1 (the seated loop). Used for play once (2026-10-18).
    // GAME CAMERA YAW (2026-10-27): [WorldChrMan] +0x17610 CS::ChrCam, +0x60 CS::ChrExFollowCam (the third person camera; the
    //   copy routine's source is a CSPersCam built from it). Yaw = atan2(fwd x, fwd z) at +0x154, +0x170 and +0x184 (pitch 4
    //   bytes before each). Writing one gets rebuilt from the others; writing all three every frame turns the camera and it
    //   stays. Used to make character movement relative to the free cam for keyboard and pads.
    // NO GRAVITY: CSChrPhysicsModule +0x1E7 byte (ER +0x1D3). Found by flipping bytes and checking the player stops falling.
    // +0x1EA also holds the player in the air (unknown what else it does).

    // CHARACTER FLAGS (found live 2026-10-18)
    //   WorldChrMan +0x17B98 / +0x17BA0: begin / end of all ChrIns* (hub and runs). +0x17E60 is only filled in the hub.
    //   CSChrDataModule +0x19B bit 0 = no death (works, hp stops at 1). Bit 1 (ER no damage) does nothing; refill hp (+0x140 = +0x144) instead.
    //   ChrIns +0x74 = team type (1 player, 6 enemy, 47 = helpers attached to the player). Same team: their hits do no damage, they still attack.
    //   CSChrBehaviorModule +0x1818 = animation speed float (ER 0x17C8). 0 = frozen statue, can't attack.
    //   Enemies ignore you: keep every enemy's team = the player's team, every frame (Studio::updateEnemyTeams). Tested on
    //   Gladius: alive, moving, never targets you. Teams seen on enemies: 0, 6, 7, 11, 30, 33. Freeze (above) was dropped for this.
    //   Nightreign EMEVD has DisableCharacterAI/EnableCharacterAI; the flag it flips wasn't found (diff attempt missed the window).
    //   ER's ChrIns +0x538 flag block (no hit / no attack / no move) is NOT at +0x560 in NR: that's a number. Not found yet.

    // EVENT FLAGS (found live 2026-10-18). CS::CSFD4VirtualMemoryFlag at global RVA 0x3C115A8 (divisor +0x1C = 1000).
    //   "48 8B 3D | ?? ?? ?? ?? 83 F9 FF 41 8B C4 0F 45 C1 89 44 24" (2 matches, same global)
    //   bool GetEventFlag(mgr, id)        RVA 0x60CE40  "44 8B 41 1C 44 8B DA 33 D2 41 8B C3 41 F7 F0"
    //   void SetEventFlag(mgr, id, state) RVA 0x60D330  "48 89 5C 24 08 44 8B 49 1C 44 8B D2 33 D2 41 8B C2 41 F7 F1"
    // Run state flags 75xx (day N started / boss started / boss defeated, 7522 last boss warp) are RECORDS: setting them mid-run
    // does nothing visible. Day progression runs on its own state machine. ER's Lua grace warp (LuaWarp) doesn't exist in NR.
    // Plan: mod EMEVD (WarpPlayer / ChangeCharacterEnableState) that waits on a borrowed unused flag (e.g. 7514), set from the app.

    // ---------------------------------------------------------------------------------
    // NOT PORTED YET (no match in NR; not needed for free cam / dolly / pause)
    //   PLAYER_COORDINATES_INTERCEPT   - needed only for player-relative camera paths
    //   RESOLUTION_STRUCT_LOCATION     - hotsampling
    //   VIGNETTE_DATA_READ, CA_ENABLE_BYTE_WRITE, DITHER_VALUE_READ, unnamed IQ pattern
    //   TOD_WRITE_INTERCEPT            - sun direction
    //   CAMERA_ADDRESS_INTERCEPT alt #2 (ER RVA 0x465C4F, camera position calc using +0x180/+0x184/+0x188)
}
