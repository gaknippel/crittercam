// App state: settings and key bindings (saved in localStorage), the link to the camera dll, and its log.
import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { BINDINGS, Binding, SETTINGS, SettingValue, encodeBinding, encodeSetting, encodePlayAnimation, encodeFloating, encodeInvincible, encodeEnemiesPassive, encodeSetFlag, encodeGetFlag, encodeLift, LOG_NOTIFICATION, CAMERA_STATE, SETTING_CHANGED, GameState } from "./protocol";

const STORAGE_KEY = "crittercam.v1";

type Saved = { settings: Record<number, SettingValue>; bindings: Record<number, Binding> };

export const defaultSettings = () =>
  Object.fromEntries(Object.values(SETTINGS).map((d) => [d.id, d.fallback])) as Record<number, SettingValue>;
export const defaultBindings = () =>
  Object.fromEntries(BINDINGS.map((b) => [b.id, { vk: b.fallback, alt: false, ctrl: false, shift: false }])) as Record<number, Binding>;

function load(): Saved {
  let saved: Partial<Saved> = {};
  try {
    saved = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? "{}");
  } catch {
    // corrupt or blocked storage: start from defaults
  }
  return { settings: { ...defaultSettings(), ...saved.settings }, bindings: { ...defaultBindings(), ...saved.bindings } };
}

export type Status = { game_running: boolean; injected: boolean; connected: boolean; pipe_blocked: boolean };
export type LogLine = { kind: number; text: string; time: string; seq: number };

const send = (message: number[]) => invoke("send", { message }).catch(() => {}); // not connected: it's sent on connect instead

export function useCrittercam() {
  const [saved, setSaved] = useState(load);
  const [status, setStatus] = useState<Status>({ game_running: false, injected: false, connected: false, pipe_blocked: false });
  const [log, setLog] = useState<LogLine[]>([]);
  const [toasts, setToasts] = useState<LogLine[]>([]);
  const [game, setGame] = useState<GameState | null>(null);
  const [update, setUpdate] = useState<{ version: string; notes?: string } | null>(null);
  const savedRef = useRef(saved);
  savedRef.current = saved;
  const seq = useRef(0);

  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(saved));
    } catch {
      // storage unavailable: settings just won't persist
    }
  }, [saved]);

  const pushAll = useCallback(() => {
    const { settings, bindings } = savedRef.current;
    Object.values(SETTINGS).forEach((d) => send(encodeSetting(d, settings[d.id])));
    BINDINGS.forEach((b) => send(encodeBinding(b.id, bindings[b.id])));
  }, []);

  const toast = useCallback((line: LogLine) => {
    setToasts((t) => [...t.slice(-3), line]);
    setTimeout(() => setToasts((t) => t.filter((x) => x.seq !== line.seq)), 2800);
  }, []);

  const addLog = useCallback((kind: number, text: string) => {
    const line = { kind, text, time: new Date().toLocaleTimeString([], { hour12: false }), seq: seq.current++ };
    setLog((l) => [...l.slice(-499), line]);
    if (kind === LOG_NOTIFICATION) toast(line);
  }, [toast]);

  // the dll already applied it; just remember it (the sidebar speed bar follows)
  const settingFromGame = useCallback((text: string) => {
    const [id, value] = text.split("=").map(Number);
    if (!SETTINGS[id] || !Number.isFinite(value)) return;
    setSaved((s) => ({ ...s, settings: { ...s.settings, [id]: value } }));
  }, []);

  useEffect(() => {
    let alive = true;
    const poll = () => invoke<Status>("status").then((s) => alive && setStatus(s)).catch(() => {});
    poll();
    const timer = setInterval(poll, 1500);
    // a reload in dev can miss the link event: if we're already connected, push everything once.
    invoke<Status>("status").then((s) => s.connected && pushAll()).catch(() => {});
    const unlisten = [
      listen<boolean>("link", (e) => {
        setStatus((s) => ({ ...s, connected: e.payload, injected: e.payload || s.injected }));
        if (!e.payload) setGame(null);
        if (e.payload) {
          pushAll();
          addLog(4, "Connected to the camera, settings sent.");
        }
      }),
      listen<{ kind: number; text: string }>("log", (e) => {
        if (e.payload.kind === SETTING_CHANGED) return settingFromGame(e.payload.text);
        if (e.payload.kind !== CAMERA_STATE) return addLog(e.payload.kind, e.payload.text);
        try {
          setGame(JSON.parse(e.payload.text));
        } catch {
          // a cut-off snapshot (huge path): keep showing the last good one
        }
      }),
    ];
    return () => {
      alive = false;
      clearInterval(timer);
      unlisten.forEach((u) => u.then((f) => f()));
    };
  }, [pushAll, addLog, settingFromGame]);

  // installed builds check the private github releases once at startup; dev builds have no token and get null
  useEffect(() => {
    invoke<{ version: string; notes?: string } | null>("check_update").then(setUpdate).catch((e) => addLog(5, `update check failed: ${e}`));
  }, [addLog]);

  const setSetting = (id: number, value: SettingValue) => {
    setSaved((s) => ({ ...s, settings: { ...s.settings, [id]: value } }));
    send(encodeSetting(SETTINGS[id], value));
  };

  const setBinding = (id: number, binding: Binding) => {
    setSaved((s) => ({ ...s, bindings: { ...s.bindings, [id]: binding } }));
    send(encodeBinding(id, binding));
  };

  const resetBindings = () => {
    const bindings = defaultBindings();
    setSaved((s) => ({ ...s, bindings }));
    BINDINGS.forEach((b) => send(encodeBinding(b.id, bindings[b.id])));
  };

  const studio = { play: (eventName: string) => send(encodePlayAnimation(eventName)), float: (on: boolean) => send(encodeFloating(on)), invincible: (on: boolean) => send(encodeInvincible(on)), enemiesPassive: (on: boolean) => send(encodeEnemiesPassive(on)), lift: (m: number) => send(encodeLift(m)) };

  const flags = { set: (flag: number, on: boolean) => send(encodeSetFlag(flag, on)), get: (flag: number) => send(encodeGetFlag(flag)) };

  const inject = () => invoke("inject").then(() => addLog(4, "Camera injected, waiting for it to connect..."));

  return { ...saved, status, log, toasts, game, update, studio, flags, installUpdate: () => invoke("install_update"), setSetting, setBinding, resetBindings, inject, clearLog: () => setLog([]) };
}
