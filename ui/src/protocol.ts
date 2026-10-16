// Messages to the camera dll. The ids must match SettingType in CameraDll/Settings.h and ActionType in CameraDll/ActionData.h.

const MESSAGE_SETTING = 1;
const MESSAGE_KEYBINDING = 2;

export const LOG_NOTIFICATION = 3;
export const LOG_NORMAL = 4;
export const LOG_ERROR = 5;
export const LOG_DEBUG = 6;

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
  rotationSpeed: 5, invertY: 6, fovSpeed: 7, dollySeconds: 8, dollyLoop: 9,
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
