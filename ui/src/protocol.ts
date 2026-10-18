// Messages to the camera dll. The ids must match SettingType in CameraDll/Settings.h and ActionType in CameraDll/ActionData.h.

const MESSAGE_SETTING = 1;
const MESSAGE_KEYBINDING = 2;
const MESSAGE_ACTION = 7; // [7, ActionMessageType, payload...], see ActionMessageType in CameraDll/Defaults.h

export const encodePlayAnimation = (eventName: string) => [MESSAGE_ACTION, 2, ...new TextEncoder().encode(eventName)];
export const encodeFloating = (on: boolean) => [MESSAGE_ACTION, 3, on ? 1 : 0];
export const encodeInvincible = (on: boolean) => [MESSAGE_ACTION, 5, on ? 1 : 0];
export const encodeEnemiesPassive = (on: boolean) => [MESSAGE_ACTION, 6, on ? 1 : 0]; // enemies kept on your team: they ignore you
const u32 = (n: number) => Array.from(new Uint8Array(new Uint32Array([n]).buffer));
export const encodeSetFlag = (flag: number, on: boolean) => [MESSAGE_ACTION, 7, ...u32(flag), on ? 1 : 0];
export const encodeGetFlag = (flag: number) => [MESSAGE_ACTION, 8, ...u32(flag)]; // answered with a notification

// the behavior event that drops any gesture (looping ones too) back to standing
export const IDLE_EVENT = "W_Idle";
export function encodeLift(meters: number): number[] {
  const payload = new DataView(new ArrayBuffer(4));
  payload.setFloat32(0, meters, true);
  return [MESSAGE_ACTION, 4, ...new Uint8Array(payload.buffer)];
}

// Nightreign's GestureParam (read from memory 2026-10-18): animation id = W_Event<id>, name = the param row name.
export const GESTURES: [number, string][] = [
  [80000, "bow"],
  [80010, "polite bow"],
  [80020, "my thanks"],
  [80030, "curtsy"],
  [80040, "reverential bow"],
  [80050, "my lord"],
  [80060, "warm welcome"],
  [80070, "wave"],
  [80080, "casual greeting"],
  [80090, "strength!"],
  [80100, "as you wish"],
  [80200, "point forwards"],
  [80210, "point upwards"],
  [80220, "point downwards"],
  [80230, "beckon"],
  [80240, "wait!"],
  [80250, "calm down!"],
  [80300, "nod in thought"],
  [80400, "extreme repentance"],
  [80410, "grovel for mercy"],
  [80500, "rallying cry"],
  [80510, "heartening cry"],
  [80520, "by my sword"],
  [80530, "hoslow's oath"],
  [80540, "fire spur me"],
  [80600, "bravo!"],
  [80700, "jump for joy"],
  [80710, "triumphant delight"],
  [80720, "fancy spin"],
  [80730, "finger snap"],
  [80800, "dejection"],
  [80900, "patches' crouch"],
  [80910, "crossed legs"],
  [80920, "rest"],
  [80930, "sitting sideways"],
  [80940, "dozing cross-legged"],
  [80950, "spread out"],
  [80970, "balled up"],
  [80980, "what do you want?"],
  [81000, "prayer"],
  [81010, "desperate prayer"],
  [81020, "rapture"],
  [81030, "erudition"],
  [81040, "outer order"],
  [81050, "inner order"],
  [81060, "golden order totality"],
  [81300, "prattling pate \"hello\""],
  [81310, "prattling pate \"thank you\""],
  [81320, "prattling pate \"apologies\""],
  [81330, "prattling pate \"wonderful\""],
  [81340, "prattling pate \"please help\""],
  [81350, "prattling pate \"my beloved\""],
  [81360, "prattling pate \"let's get to it\""],
  [81370, "prattling pate \"you're beautiful\""],
  [81400, "it's raining!"],
];

export const LOG_NOTIFICATION = 3;
export const LOG_NORMAL = 4;
export const LOG_ERROR = 5;
export const LOG_DEBUG = 6;
export const SETTING_CHANGED = 9; // "id=value" from the dll: a setting changed in game (shift + wheel game speed)
export const CAMERA_STATE = 8; // json snapshot from the dll, see System::sendCameraState

// cam: [x, y, z, yaw, pitch, roll, fov] (meters, degrees), null while our camera is off. nodes: [x, y, z, yaw, pitch, fov].
// me: the local player [x, y, z]; chrs: everyone else alive [x, y, z, isPlayer]. Both missing when the character list isn't readable.
export type GameState = { on: number; paused: number; playing: number; hud: number; loop: number; speed?: number; cam: number[] | null; nodes: number[][]; curve: number[][]; me?: number[]; chrs?: number[][] };

export type FloatSetting = {
  kind: "float"; id: number; label: string; hint: string;
  min: number; max: number; decimals: number; log?: boolean; fallback: number;
};
export type ChoiceSetting = { kind: "choice"; id: number; label: string; hint: string; options: string[]; fallback: number };
export type BoolSetting = { kind: "bool"; id: number; label: string; hint: string; fallback: boolean };
export type SettingDef = FloatSetting | ChoiceSetting | BoolSetting;
export type SettingValue = number | boolean;

export const S = {
  fastMultiplier: 0, slowMultiplier: 1, upMultiplier: 2, movementSpeed: 3, controlDevice: 4,
  rotationSpeed: 5, invertY: 6, fovSpeed: 7, dollySeconds: 8, dollyLoop: 9, gameSpeed: 10,
} as const;

export const SETTINGS: Record<number, SettingDef> = {
  [S.movementSpeed]: { kind: "float", id: S.movementSpeed, label: "movement speed", hint: "base flying speed", min: 0.001, max: 0.5, decimals: 3, log: true, fallback: 0.05 },
  [S.rotationSpeed]: { kind: "float", id: S.rotationSpeed, label: "look speed", hint: "keys, sticks and mouse", min: 0.001, max: 0.5, decimals: 3, log: true, fallback: 0.01 },
  [S.fastMultiplier]: { kind: "float", id: S.fastMultiplier, label: "fast multiplier", hint: "hold alt, or y on a pad", min: 0.1, max: 100, decimals: 2, log: true, fallback: 5 },
  [S.slowMultiplier]: { kind: "float", id: S.slowMultiplier, label: "slow multiplier", hint: "hold ctrl, or x on a pad", min: 0.001, max: 1, decimals: 3, log: true, fallback: 0.1 },
  [S.upMultiplier]: { kind: "float", id: S.upMultiplier, label: "up / down multiplier", hint: "relative to movement speed", min: 0.1, max: 10, decimals: 2, log: true, fallback: 0.7 },
  [S.fovSpeed]: { kind: "float", id: S.fovSpeed, label: "zoom speed", hint: "keys and mouse wheel", min: 0.01, max: 1, decimals: 2, fallback: 0.3 },
  [S.controlDevice]: { kind: "choice", id: S.controlDevice, label: "camera control", hint: "the other device keeps playing the character", options: ["keyboard & mouse", "controller", "both"], fallback: 2 },
  [S.invertY]: { kind: "bool", id: S.invertY, label: "invert look up / down", hint: "", fallback: false },
  [S.dollySeconds]: { kind: "float", id: S.dollySeconds, label: "seconds per node", hint: "time between two keyframes", min: 0.1, max: 60, decimals: 1, log: true, fallback: 3 },
  [S.gameSpeed]: { kind: "float", id: S.gameSpeed, label: "game speed", hint: "or hold shift + scroll in game", min: 0.05, max: 4, decimals: 2, log: true, fallback: 1 },
  [S.dollyLoop]: { kind: "bool", id: S.dollyLoop, label: "loop path", hint: "fly from the last node back to the first", fallback: false },
};

export type Binding = { vk: number; alt: boolean; ctrl: boolean; shift: boolean };
export type BindingDef = { id: number; label: string; group: string; fallback: number };

// HLAE style, no numpad needed.
export const BINDINGS: BindingDef[] = [
  { id: 1, label: "camera on / off", group: "general", fallback: 45 },
  { id: 5, label: "lock camera movement", group: "general", fallback: 145 },
  { id: 0, label: "block game input", group: "general", fallback: 8 },
  { id: 25, label: "cycle camera target", group: "general", fallback: 120 },
  { id: 8, label: "forward", group: "move", fallback: 87 },
  { id: 9, label: "back", group: "move", fallback: 83 },
  { id: 6, label: "left", group: "move", fallback: 65 },
  { id: 7, label: "right", group: "move", fallback: 68 },
  { id: 10, label: "up", group: "move", fallback: 82 },
  { id: 11, label: "down", group: "move", fallback: 70 },
  { id: 12, label: "look up", group: "look", fallback: 38 },
  { id: 13, label: "look down", group: "look", fallback: 40 },
  { id: 14, label: "look left", group: "look", fallback: 37 },
  { id: 15, label: "look right", group: "look", fallback: 39 },
  { id: 16, label: "tilt left", group: "look", fallback: 90 },
  { id: 17, label: "tilt right", group: "look", fallback: 88 },
  { id: 2, label: "zoom in", group: "lens", fallback: 33 },
  { id: 3, label: "zoom out", group: "lens", fallback: 34 },
  { id: 4, label: "reset zoom", group: "lens", fallback: 36 },
  { id: 21, label: "add node", group: "dolly", fallback: 116 },
  { id: 22, label: "play / stop", group: "dolly", fallback: 117 },
  { id: 23, label: "delete last node", group: "dolly", fallback: 118 },
  { id: 24, label: "clear path", group: "dolly", fallback: 119 },
  { id: 19, label: "pause / unpause", group: "game", fallback: 19 },
  { id: 20, label: "skip frames (paused)", group: "game", fallback: 35 },
  { id: 18, label: "hud and path on / off", group: "game", fallback: 46 },
];

export function encodeSetting(def: SettingDef, value: SettingValue): number[] {
  const payload = new DataView(new ArrayBuffer(4));
  let bytes: number[];
  if (def.kind === "bool") {
    bytes = [value ? 1 : 0];
  } else {
    if (def.kind === "float") payload.setFloat32(0, value as number, true);
    else payload.setInt32(0, value as number, true);
    bytes = Array.from(new Uint8Array(payload.buffer));
  }
  return [MESSAGE_SETTING, def.id, ...bytes];
}

export function encodeBinding(id: number, b: Binding): number[] {
  return [MESSAGE_KEYBINDING, id, b.vk, +b.alt, +b.ctrl, +b.shift];
}

// KeyboardEvent.code -> Windows virtual key code + label.
const KEYS: [string, number, string][] = [
  ["Backspace", 8, "Backspace"], ["Tab", 9, "Tab"], ["Enter", 13, "Enter"], ["Pause", 19, "Pause"], ["CapsLock", 20, "Caps Lock"],
  ["Space", 32, "Space"], ["PageUp", 33, "Page Up"], ["PageDown", 34, "Page Down"], ["End", 35, "End"], ["Home", 36, "Home"],
  ["ArrowLeft", 37, "←"], ["ArrowUp", 38, "↑"], ["ArrowRight", 39, "→"], ["ArrowDown", 40, "↓"], ["PrintScreen", 44, "Print Screen"],
  ["Insert", 45, "Insert"], ["Delete", 46, "Delete"], ["ScrollLock", 145, "Scroll Lock"],
  ["Semicolon", 186, ";"], ["Equal", 187, "="], ["Comma", 188, ","], ["Minus", 189, "-"], ["Period", 190, "."], ["Slash", 191, "/"],
  ["Backquote", 192, "`"], ["BracketLeft", 219, "["], ["Backslash", 220, "\\"], ["BracketRight", 221, "]"], ["Quote", 222, "'"],
  ["NumpadMultiply", 106, "Num *"], ["NumpadAdd", 107, "Num +"], ["NumpadSubtract", 109, "Num -"], ["NumpadDecimal", 110, "Num ."], ["NumpadDivide", 111, "Num /"],
];
for (let i = 0; i < 26; i++) { const c = String.fromCharCode(65 + i); KEYS.push([`Key${c}`, 65 + i, c]); }
for (let i = 0; i < 10; i++) { KEYS.push([`Digit${i}`, 48 + i, `${i}`], [`Numpad${i}`, 96 + i, `Num ${i}`]); }
for (let i = 1; i <= 24; i++) KEYS.push([`F${i}`, 111 + i, `F${i}`]);

const VK_BY_CODE = new Map(KEYS.map(([code, vk]) => [code, vk]));
const LABEL_BY_VK = new Map(KEYS.map(([, vk, label]) => [vk, label]));

export const vkFromCode = (code: string) => VK_BY_CODE.get(code);
export const keyLabel = (vk: number) => LABEL_BY_VK.get(vk) ?? `Key ${vk}`;
