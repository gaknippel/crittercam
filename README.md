# crittercam

Free camera, dolly paths, pause, and HUD toggle for **ELDEN RING NIGHTREIGN** (PC). Personal use only.

Built on [Injectable Generic Camera System (IGCS)](https://github.com/FransBouma/InjectableGenericCameraSystem) by Frans "Otis_Inf" Bouma (BSD-2, see `LICENSE-IGCS.txt`). Uses [MinHook](https://github.com/TsudaKageyu/minhook). The byte patterns were ported from Otis_Inf's Elden Ring Camera Tools, so **don't publish this repo or the DLL**.

> Offline only. Run the game with EAC disabled (`steam_appid.txt` in the `Game` folder, start `nightreign.exe` directly). Never use this online.

## How it works

```
nightreign.exe                       crittercam.dll (injected)
┌──────────────────────────┐         ┌──────────────────────────────┐
│ camera select routine    │──hook──▶│ asm: swap source → our buffer │
│  copies source camera    │         └──────────────┬───────────────┘
│  into the render camera  │◀── our camera ─────────┘
└──────────────────────────┘                 ▲
                                             │ writes 2 buffers, flips (no tearing)
crittercam client ──settings/keys (pipe)──▶ System loop: free cam / dolly playback
```

- **Camera:** the game copies one of several source cameras into the render camera every frame. We hook that copy, and when the camera is on, point it at our own camera struct (matrix at `+0x10`, FoV at `+0x50`).
- **Pause:** zeroes the game speed in the time dilation struct (the code checks which of the two floats is the speed).
- **HUD:** *experimental.* Zeroes the 32 byte value Otis's Elden Ring tool hooks for HUD opacity.
- **Dolly:** keyframes you place with the free cam. Played back with a Catmull-Rom spline (position, FoV) and quaternion squad (rotation), with ease-in/out.

## Build

1. Open `crittercam.sln` in Visual Studio 2022 (needs the *Desktop development with C++* workload).
2. Set the toolbar to **Release** / **x64**.
3. **Build → Build Solution** (Ctrl+Shift+B). NuGet restores the client's packages automatically the first time.
4. Output: `bin\x64\Release\crittercam.dll`.
5. Client (Tauri): `cd ui`, `npm install` once, then `npm run tauri dev` to run it, or `npm run release` to build `ui\src-tauri\target\release\crittercam.exe`.

## Use

1. Start Nightreign offline (EAC disabled) and load in.
2. Run the crittercam client (see Build) and click **Inject camera**. Keep it open, it's where settings and keys live. Close the old `IGCSClient.exe` first: both can't run at once.
3. Press **Insert** to enable the camera.

| Key | Action |
|---|---|
| Insert | Camera on / off |
| W / S / A / D | Move forward / back / left / right |
| R / F | Move up / down |
| Arrow keys, or mouse | Rotate |
| Z / X | Tilt left / right |
| Page Up / Page Down / Home, or mouse wheel | Zoom in / out / reset |
| Alt / Ctrl (hold) | Faster / slower |
| Pause | Pause / unpause |
| End | Skip a few frames while paused |
| Delete | HUD toggle (experimental), also hides the dolly path |
| Backspace | Block game input (on by default while the camera is on) |
| Scroll Lock | Lock camera movement |
| **F5** | Dolly: add node at the camera |
| **F6** | Dolly: play / stop |
| **F7** | Dolly: delete last node |
| **F8** | Dolly: clear path |
| **F9** | Cycle camera target (see below) |

While the free cam is on, the dolly path is drawn over the game: orange curve, numbered nodes, cyan line = where each node looks. Needs **Borderless windowed** (or Windowed) display mode.

Dolly timing (seconds per node) and looping are on the client's Dolly page. All keys can be rebound in the client.

**Dolly workflow:** pause (Pause), enable camera, fly to a spot, F5, fly to the next spot, F5 … then F6.

## First test checklist

None of this has been tested in game yet. Things to check, in order:

1. **Injection.** The client's log should say `All interception offsets found.` and `Camera found.` If a pattern isn't found, a patch changed the code (see `docs/NightreignAOBs.h`).
2. **Camera moves?** If enabling the camera freezes the view but movement does nothing, the routine writes to several cameras and we picked the wrong one. Press **F9** until it moves. The log lists every camera it saw.
3. **Directions right?** If up/down or left/right are mirrored, the conventions are all in `Camera.cpp` (comments explain each sign).
4. **Pause.** Pause key. The log shows which float got zeroed (`+0x2D4` or `+0x270`). If the game keeps running, swap the offsets in `GameConstants.h`.
5. **Crash on inject or after a while?** Nightreign's code protection may detect the patched code. That's the known risk with this approach; report back what happens and when.

## Project layout

| Path | What |
|---|---|
| `CameraDll/` | The injected DLL (C++ / MASM) |
| `CameraDll/GameConstants.h` | Game specific offsets and keys |
| `CameraDll/InterceptorHelper.cpp` | Byte patterns + hooks |
| `CameraDll/Interceptor.asm` | The hook code that runs inside the game |
| `CameraDll/CameraManipulator.cpp` | Writes our camera into the game, pause |
| `CameraDll/Camera.cpp` | Free camera math |
| `CameraDll/CameraPath.cpp` | Dolly paths |
| `ui/` | Client: injector + settings window (Tauri: React/TypeScript + Rust) |
| `IGCSClient/` | Old client (C# / WPF), replaced by `ui/` |
| `dependencies/` | MinHook |
| `docs/NightreignAOBs.h` | Notes on how each pattern was ported from Elden Ring |
