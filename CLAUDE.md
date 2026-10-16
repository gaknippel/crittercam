# crittercam: context for Claude Code

Camera mod for **ELDEN RING NIGHTREIGN** (PC): free cam, dolly paths, pause, HUD toggle. Personal use only, offline only (EAC disabled). The repo must stay private: byte patterns were ported from Otis_Inf's closed-source Elden Ring Camera Tools (the user owns a copy and knows Otis). Never suggest publishing it.

Built on Otis_Inf's open-source IGCS framework (BSD-2, `LICENSE-IGCS.txt`), copied from IGCS's `Cameras/ResidentEvil3` template, plus vendored MinHook.

## Status

- `crittercam.dll` builds clean in VS 2022 (Release|x64). Only warnings are pre-existing IGCS ones.
- Client (`IGCSClient`, C# WPF) was retargeted to .NET Framework 4.8. It didn't build at first because VS was missing the **.NET desktop development** workload (project showed as not loaded, NuGet said "nothing to restore").
- **Nothing has been tested in game yet.** First test checklist is in `README.md`.

## Build

- Open `crittercam.sln` in Visual Studio 2022, Release | x64, Ctrl+Shift+B.
- Needs workloads: *Desktop development with C++* and *.NET desktop development*.
- Post-build copies `crittercam.dll` to `IGCSClient\bin\Release\`, next to `IGCSClient.exe`.
- Can't be built from VS Code: build in Visual Studio (or `msbuild` from a Developer PowerShell).

## Layout

| Path | What |
|---|---|
| `CameraDll/GameConstants.h` | Offsets, AOB keys, tuning constants |
| `CameraDll/InterceptorHelper.cpp` | AOB patterns + hook install |
| `CameraDll/Interceptor.asm` | MASM hooks that run inside the game |
| `CameraDll/CameraManipulator.cpp` | Double-buffered camera write, camera target pick/cycle, pause |
| `CameraDll/Camera.cpp` | Free cam math (all sign conventions live here) |
| `CameraDll/CameraPath.cpp` | Dolly: Catmull-Rom position/FoV, quaternion squad rotation, smootherstep |
| `CameraDll/System.cpp` | Main loop, input, dolly keys |
| `CameraDll/PathOverlay.cpp` | Dolly path drawn in a transparent click-through window owned by the game window (GDI, color key). Projects with our camera from `g_cameraOverrideBuffer`, assumes vertical FoV. Needs borderless/windowed. Shown while free cam is on, hidden during playback and with the HUD toggle |
| `CameraDll/Utils.cpp` | AOB scanner (rewritten to skip unreadable pages) |
| `ui/` | **New client** (Tauri 2: React + TypeScript UI, Rust backend). `src/protocol.ts` = setting/key ids, defaults, message encoding; `src-tauri/src/pipes.rs` = both named pipes; `game.rs` = find game, LoadLibraryW injection. Settings live in the webview's localStorage. Run: `cd ui; npm run tauri dev`, or `npm run release` → `ui/src-tauri/target/release/crittercam.exe`. Finds the dll next to the exe or in `bin/x64/Release` above it |
| `IGCSClient/` | Old C# WPF client, kept until `ui/` is verified in game. Can't run at the same time as `ui/` (same pipe name) |
| `docs/NightreignAOBs.h` | How each pattern was ported from Elden Ring |

## How the camera works

The game has a camera-select routine that copies one of several source cameras (`rcx`) into the render camera (`[r9]`). Nightreign's protection layer splits it into chunks joined by `jmp`s.

- Hook at `nightreign.exe+1A40C27`, pattern `8B 41 50 49 8B 11 89 42 50 8B 41 54 89 42 54 8B 41 58 E9`. The 14-byte jmp overwrites 5 instructions (15 bytes), continue offset `0x0F`.
- When the camera is on and the destination equals `g_activeCameraStructAddress`, the asm copies aspect/near/far into our buffer and sets `rcx` = `g_cameraOverrideBuffer`, so the rest of the routine copies OUR camera. `rcx` isn't used after the copy (routine ends `and [r9+20],-40; ret`).
- While the camera is off, the asm records up to 8 distinct destinations in `g_cameraCandidates`. On enable, C++ clears the table, sleeps 100 ms, and picks one. **F9** cycles if the wrong one was picked.
- C++ writes poses into two buffers and flips the pointer with `InterlockedExchangePointer` (no torn matrices).
- A second branch of the routine (flag `0x20`, at `+5E32259`) copies cameras without passing our hook. If the free cam flickers in some states, that's the suspect.

Camera struct (same as Elden Ring): `+0x10..0x4F` 4x4 row-major matrix (rows: right, up, forward, position w=1), `+0x50` FoV (radians), `+0x54` aspect, `+0x58` near, `+0x5C` far.

Conventions: DirectX left-handed, +X right, +Y up, +Z forward. Rotation = `XMMatrixRotationRollPitchYaw(pitch, yaw, roll)`. Positive pitch looks down (so `Camera::pitch` subtracts), positive yaw turns right, positive roll tilts left. Up/down movement uses world up. Matrix→angles: `pitch = asin(-f.y)`, `yaw = atan2(f.x, f.z)`, `roll = atan2(right.y, up.y)`. These were verified with 1155 round-trip tests against real DirectXMath, but **not yet against the game's real matrix layout**.

## Other hooks

- **Pause:** pattern `48 8B 05 | ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 59 88 ?? ?? ?? ?? 48 8D` (3 matches, all the same global, RVA `0x442FEF8`). The struct has two floats the game multiplies (`+0x2D4`, `+0x270`; ER used `+0x2CC`/`+0x268`). The code zeroes whichever one is 1.0 at pause time, re-asserts it every frame, and re-reads the global pointer each time.
- **HUD (experimental):** replaces the 20-byte leaf function at `+12C5C60` (`41 0F 28 00 48 8B C2 41 0F 28 48 ?? 0F 29 02 0F 29 4A`) and returns zeros when hidden. Guessed from Otis's ER hook name `HUD_OPACITY_READ`; might be shared by other code.
- Otis's real engine pause (`C1 E8 12 A8 01 74 ?? 48 8B 0D ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 48 85 C9 74 05`, RVA `0x566F31`) was found but isn't used yet.
- **Controller → character while free cam is on:** the game imports `XINPUT1_4` (by ordinal), so `InputHooker` hooks that (1_3 fallback). The asm stores the game's own camera in `g_gameSourceCamera`, and the XInput detour rotates the left stick by (free cam yaw − game cam yaw) so the character moves relative to the view. Keyboard → character is not rotated (game reads it via DirectInput/GetKeyboardState).
- Not ported: player coords, hotsampling resolution, vignette, chromatic aberration, dither, time of day.

## Keys and settings

ActionType ids in `CameraDll/ActionData.h` must match `BINDINGS` in `ui/src/protocol.ts` (and `KeyBindingType` in the old `IGCSClient/ConstantsEnums.cs`) (dolly keys are 21–25: add node F5, play/stop F6, delete last F7, clear F8, cycle camera target F9). SettingType 8/9 were repurposed from RE3's resolution scale/sharpening to `DollySegmentSeconds`/`DollyLoop`; keep `SETTINGS` in `ui/src/protocol.ts` in sync too.

## Known risks to watch in the first test

1. Wrong destination camera → F9.
2. Mirrored axes → signs in `Camera.cpp`.
3. Pause offset wrong → swap in `GameConstants.h`.
4. Crash on inject or later → Nightreign's code integrity checks may detect the patched bytes. Biggest unknown.

## Gotchas

- IGCS's `Utils::clamp(v, min, max, default)` returns `default` when out of range, it doesn't clamp. Use `std::clamp`.
- `windows.h` defines `min`/`max` macros: write `(std::min)(a, b)`.
- `GameImageHooker::setHook` writes a 14-byte `jmp qword ptr [rip]`: any new hook site needs ≥14 bytes of whole instructions with no jump targets inside.
- AOB scanner skips no-access/guard pages: keep it that way.

## About the user

CS student, knows React and full-stack web, comfortable in VS Code, new to Visual Studio and C++ game modding. Wants short, plain answers and visual explanations.
