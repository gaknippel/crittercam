// App state: settings and key bindings (saved in localStorage), the link to the camera dll, and its log.
import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { BINDINGS, Binding, SETTINGS, SettingValue, encodeBinding, encodeSetting, LOG_NOTIFICATION } from "./protocol";

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

  const addLog = useCallback((kind: number, text: string) => {
    const line = { kind, text, time: new Date().toLocaleTimeString([], { hour12: false }), seq: seq.current++ };
    setLog((l) => [...l.slice(-499), line]);
    if (kind === LOG_NOTIFICATION) {
      setToasts((t) => [...t.slice(-3), line]);
      setTimeout(() => setToasts((t) => t.filter((x) => x.seq !== line.seq)), 2800);
    }
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
        if (e.payload) {
          pushAll();
          addLog(4, "Connected to the camera, settings sent.");
        }
      }),
      listen<{ kind: number; text: string }>("log", (e) => addLog(e.payload.kind, e.payload.text)),
    ];
    return () => {
      alive = false;
      clearInterval(timer);
      unlisten.forEach((u) => u.then((f) => f()));
    };
  }, [pushAll, addLog]);

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

  const inject = () => invoke("inject").then(() => addLog(4, "Camera injected, waiting for it to connect..."));

  return { ...saved, status, log, toasts, setSetting, setBinding, resetBindings, inject, clearLog: () => setLog([]) };
}
