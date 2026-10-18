# crittercam

free cam, dolly paths, pause, timescale, and HUD toggle, real time cam data, for **ELDEN RING NIGHTREIGN** (PC). 


> OFFLINE ONLY!! run the game with EAC disabled (`steam_appid.txt` in the `Game` folder, start `nightreign.exe` directly). never use this online.

## how it works

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

- **camera:** the game copies one of several source cameras into the render camera every frame. We hook that copy, and when the camera is on, point it at our own camera struct (matrix at `+0x10`, FoV at `+0x50`).
- **pause:** zeroes the game speed in the time dilation struct (the code checks which of the two floats is the speed).
- **dolly:** keyframes you place with the free cam. played back with a catmull-rom spline (position, FoV) and quaternion squad (rotation), with ease-in/out.

## build

1. open `crittercam.sln` in Visual Studio 2022 (needs the *Desktop development with C++* workload).
2. set the toolbar to **Release** / **x64**.
3. **build → build solution** . NuGet restores the client's packages automatically the first time.
4. output: `bin\x64\Release\crittercam.dll`.
5. client (tauri): `cd ui`, `npm install` once, then `npm run tauri dev` to run it, or `npm run release` to build `ui\src-tauri\target\release\crittercam.exe`.

## updates


ship a version: `.\ship.ps1 0.2.0 "what changed"` (game closed). It builds the dll and the installer, signs it, and creates release `v0.2.0` with the installer and `latest.json`. commit the version bump afterwards.


## use

1. start nightreign offline (EAC disabled) and load in.
2. run the crittercam client and click **inject camera**. keep it open, it's where settings and keys live.
3. press **insert** to enable the camera.

| key | action |
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
| Shift + mouse wheel | Game speed slower / faster (eased; the client shows it) |
| Delete | HUD toggle (experimental), also hides the dolly path |
| Backspace | Block game input (on by default while the camera is on) |
| Scroll Lock | Lock camera movement |
| **F5** | Dolly: add node at the camera |
| **F6** | Dolly: play / stop |
| **F7** | Dolly: delete last node |
| **F8** | Dolly: clear path |
| **F9** | Cycle camera target (see below) |

while the free cam is on, the dolly path is drawn over the game: orange curve, numbered nodes, cyan line = where each node looks. Needs **borderless windowed** (or windowed) display mode.

dolly timing (seconds per node) and looping are on the client's dolly page. all keys can be rebound in the client.

**dolly instructions:** pause (pause), enable camera, fly to a spot, F5, fly to the next spot, F5 … then F6.

## first test checklist


## project layout

| path | what |
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
