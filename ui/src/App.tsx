import { useMemo, useState } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { CameraIcon, PathIcon, KeyboardIcon, TerminalIcon, MinusIcon, XIcon, SunIcon, MoonIcon, DesktopIcon } from "@phosphor-icons/react";
import { cn } from "@/lib/utils";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { ToggleGroup, ToggleGroupItem } from "@/components/ui/toggle-group";
import nightreignIcon from "./assets/nightreign.png";
import { useCrittercam, Status } from "./state";
import { useTheme, Theme } from "./theme";
import { BINDINGS, Binding, FloatSetting, LOG_DEBUG, LOG_ERROR, LOG_NOTIFICATION, S, SETTINGS } from "./protocol";
import { ChoiceRow, KeyCapture, Keys, SliderRow, SwitchRow } from "./controls";

type Page = "camera" | "dolly" | "keys" | "log";
type App = ReturnType<typeof useCrittercam>;

const PAGES: { id: Page; label: string; icon: typeof CameraIcon }[] = [
  { id: "camera", label: "camera", icon: CameraIcon },
  { id: "dolly", label: "dolly", icon: PathIcon },
  { id: "keys", label: "keys", icon: KeyboardIcon },
  { id: "log", label: "log", icon: TerminalIcon },
];

export default function App() {
  const app = useCrittercam();
  const [theme, setTheme] = useTheme();
  const [page, setPage] = useState<Page>("camera");
  const errors = app.log.filter((l) => l.kind === LOG_ERROR).length;

  return (
    <div className="grid h-screen grid-cols-[232px_1fr] grid-rows-[36px_1fr] bg-background text-foreground">
      <TitleBar />
      <aside className="flex min-h-0 flex-col gap-4 border-r p-3">
        <div className="flex items-center gap-3 px-1 pt-1">
          <img src={nightreignIcon} alt="" className="size-9" />
          <div>
            <div className="text-sm font-semibold">crittercam</div>
            <div className="text-[11px] text-muted-foreground">elden ring nightreign</div>
          </div>
        </div>
        <StatusCard status={app.status} inject={app.inject} />
        <nav className="flex flex-col gap-0.5">
          {PAGES.map(({ id, label, icon: Icon }) => (
            <Button key={id} variant={page === id ? "secondary" : "ghost"} className="justify-start gap-3" onClick={() => setPage(id)} aria-current={page === id ? "page" : undefined}>
              <Icon />
              {label}
              {id === "log" && errors > 0 && <Badge variant="destructive" className="ml-auto">{errors}</Badge>}
            </Button>
          ))}
        </nav>
        <div className="mt-auto flex flex-col gap-3">
          <ToggleGroup type="single" variant="outline" size="sm" value={theme} onValueChange={(v) => v && setTheme(v as Theme)} aria-label="theme" className="w-full">
            <ToggleGroupItem value="light" className="flex-1" aria-label="light"><SunIcon /></ToggleGroupItem>
            <ToggleGroupItem value="dark" className="flex-1" aria-label="dark"><MoonIcon /></ToggleGroupItem>
            <ToggleGroupItem value="system" className="flex-1" aria-label="system"><DesktopIcon /></ToggleGroupItem>
          </ToggleGroup>
          <div className="px-1 text-[11px] text-muted-foreground">built on igcs by otis_inf</div>
        </div>
      </aside>
      <main className="min-h-0 overflow-y-auto p-6">
        {page === "camera" && <CameraPage app={app} />}
        {page === "dolly" && <DollyPage app={app} />}
        {page === "keys" && <KeysPage app={app} />}
        {page === "log" && <LogPage app={app} />}
      </main>
      <div className="pointer-events-none fixed right-5 bottom-5 flex flex-col items-end gap-2" aria-live="polite">
        {app.toasts.map((t) => (
          <div key={t.seq} className="border bg-popover px-3 py-2 text-sm text-popover-foreground shadow-md">{t.text}</div>
        ))}
      </div>
    </div>
  );
}

function TitleBar() {
  const win = getCurrentWindow();
  return (
    <header className="col-span-2 flex items-center justify-between border-b pl-3 text-xs text-muted-foreground" data-tauri-drag-region>
      <span data-tauri-drag-region>crittercam</span>
      <div className="flex h-full">
        <button className="grid h-full w-11 place-items-center hover:bg-muted" onClick={() => win.minimize()} aria-label="minimize"><MinusIcon /></button>
        <button className="grid h-full w-11 place-items-center hover:bg-destructive hover:text-white" onClick={() => win.close()} aria-label="close"><XIcon /></button>
      </div>
    </header>
  );
}

const STATES = {
  connected: { title: "camera active", sub: "press your camera key in game", dot: "bg-green-500" },
  blocked: { title: "old client is open", sub: "close igcsclient.exe first", dot: "bg-destructive" },
  stale: { title: "already injected", sub: "restart the game to reconnect", dot: "bg-destructive" },
  ready: { title: "game running", sub: "ready to inject", dot: "bg-yellow-500" },
  waiting: { title: "waiting for game", sub: "start elden ring nightreign", dot: "bg-muted-foreground/50" },
};

function StatusCard({ status, inject }: { status: Status; inject: () => Promise<void> }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const state = status.connected ? "connected" : status.pipe_blocked ? "blocked" : status.injected ? "stale" : status.game_running ? "ready" : "waiting";
  const s = STATES[state];
  const run = () => {
    setBusy(true);
    setError("");
    inject().catch((e) => setError(String(e))).finally(() => setBusy(false));
  };
  return (
    <div className="flex flex-col gap-3 border p-3">
      <div className="flex items-center gap-3">
        <span className={cn("size-2.5 shrink-0", s.dot)} />
        <div>
          <div className="text-sm font-medium">{s.title}</div>
          <div className="text-xs text-muted-foreground">{s.sub}</div>
        </div>
      </div>
      {(state === "ready" || state === "waiting") && (
        <Button variant="outline" disabled={state !== "ready" || busy} onClick={run}>
          {busy ? "injecting…" : "inject camera"}
        </Button>
      )}
      {error && <p className="text-xs text-destructive">{error}</p>}
    </div>
  );
}

function PageHeader({ title, sub, children }: { title: string; sub: string; children?: React.ReactNode }) {
  return (
    <div className="mb-5 flex items-end justify-between gap-4">
      <div>
        <h1 className="text-lg font-semibold">{title}</h1>
        <p className="text-sm text-muted-foreground">{sub}</p>
      </div>
      {children}
    </div>
  );
}

function Section({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <Card className="mb-4 gap-0 py-0">
      <CardHeader className="border-b py-3">
        <CardTitle className="text-xs font-medium text-muted-foreground">{title}</CardTitle>
      </CardHeader>
      <CardContent className="divide-y">{children}</CardContent>
    </Card>
  );
}

function SliderFor({ app, id }: { app: App; id: number }) {
  return <SliderRow def={SETTINGS[id] as FloatSetting} value={app.settings[id] as number} onChange={(v) => app.setSetting(id, v)} />;
}

function SwitchFor({ app, id }: { app: App; id: number }) {
  const d = SETTINGS[id];
  return <SwitchRow label={d.label} hint={d.hint} value={app.settings[id] as boolean} onChange={(v) => app.setSetting(id, v)} />;
}

function CameraPage({ app }: { app: App }) {
  const device = SETTINGS[S.controlDevice];
  return (
    <>
      <PageHeader title="camera" sub="how the free cam moves. changes apply live." />
      <Section title="control">
        <ChoiceRow label={device.label} hint={device.hint} options={device.kind === "choice" ? device.options : []} value={app.settings[S.controlDevice] as number} onChange={(v) => app.setSetting(S.controlDevice, v)} />
        <SwitchFor app={app} id={S.invertY} />
      </Section>
      <Section title="speed">
        <SliderFor app={app} id={S.movementSpeed} />
        <SliderFor app={app} id={S.rotationSpeed} />
        <SliderFor app={app} id={S.upMultiplier} />
        <SliderFor app={app} id={S.fastMultiplier} />
        <SliderFor app={app} id={S.slowMultiplier} />
      </Section>
      <Section title="lens">
        <SliderFor app={app} id={S.fovSpeed} />
      </Section>
    </>
  );
}

function DollyPage({ app }: { app: App }) {
  const key = (id: number) => <Keys binding={app.bindings[id]} />;
  const steps: [string, React.ReactNode][] = [
    ["pause the game", key(19)],
    ["turn the camera on", key(1)],
    ["fly to a spot and add a node. repeat.", key(21)],
    ["play it back", key(22)],
    ["undo the last node, or clear the path", <span className="inline-flex gap-2">{key(23)}{key(24)}</span>],
    ["hide the path and hud for a clean frame", key(18)],
  ];
  return (
    <>
      <PageHeader title="dolly" sub="place keyframes with the free cam, then play them back as one smooth move." />
      <Section title="playback">
        <SliderFor app={app} id={S.dollySeconds} />
        <SwitchFor app={app} id={S.dollyLoop} />
      </Section>
      <Section title="how to">
        {steps.map(([text, keys], i) => (
          <div key={text} className="flex items-center justify-between gap-4 py-2.5 text-sm">
            <span><span className="mr-3 text-muted-foreground">{i + 1}</span>{text}</span>
            {keys}
          </div>
        ))}
        <p className="py-3 text-xs text-muted-foreground">the path shows in game while the camera is on. it needs borderless windowed or windowed display mode.</p>
      </Section>
    </>
  );
}

const comboKey = (b: Binding) => `${b.vk}|${+b.alt}|${+b.ctrl}|${+b.shift}`;

function KeysPage({ app }: { app: App }) {
  const conflicts = useMemo(() => {
    const byCombo = new Map<string, string[]>();
    BINDINGS.forEach((d) => {
      const k = comboKey(app.bindings[d.id]);
      byCombo.set(k, [...(byCombo.get(k) ?? []), d.label]);
    });
    return byCombo;
  }, [app.bindings]);
  const groups = [...new Set(BINDINGS.map((b) => b.group))];
  return (
    <>
      <PageHeader title="keys" sub="click a key, then press the new one. esc cancels. hold alt for fast, ctrl for slow.">
        <Button variant="outline" onClick={app.resetBindings}>reset to defaults</Button>
      </PageHeader>
      <div className="grid grid-cols-[repeat(auto-fill,minmax(300px,1fr))] items-start gap-x-4">
        {groups.map((g) => (
          <Section title={g} key={g}>
            {BINDINGS.filter((b) => b.group === g).map((d) => {
              const others = (conflicts.get(comboKey(app.bindings[d.id])) ?? []).filter((l) => l !== d.label);
              return (
                <div key={d.id} className="flex items-center justify-between gap-4 py-1.5 text-sm">
                  <span>{d.label}</span>
                  <KeyCapture binding={app.bindings[d.id]} onChange={(b) => app.setBinding(d.id, b)} conflict={others.join(", ") || undefined} />
                </div>
              );
            })}
          </Section>
        ))}
      </div>
    </>
  );
}

function LogPage({ app }: { app: App }) {
  const [debug, setDebug] = useState(false);
  const lines = app.log.filter((l) => debug || l.kind !== LOG_DEBUG);
  const color = (k: number) => (k === LOG_ERROR ? "text-destructive" : k === LOG_DEBUG ? "text-muted-foreground" : k === LOG_NOTIFICATION ? "text-primary font-medium" : "");
  return (
    <>
      <PageHeader title="log" sub="messages from the camera inside the game, newest first.">
        <div className="flex gap-2">
          <Button variant={debug ? "secondary" : "outline"} onClick={() => setDebug(!debug)} aria-pressed={debug}>debug</Button>
          <Button variant="outline" onClick={app.clearLog}>clear</Button>
        </div>
      </PageHeader>
      <Card className="py-2 select-text">
        {lines.length === 0 && <p className="px-4 py-2 text-sm text-muted-foreground">nothing yet. inject the camera to see what it's doing.</p>}
        {[...lines].reverse().map((l) => (
          <div key={l.seq} className="flex gap-4 px-4 py-0.5 text-xs hover:bg-muted/50">
            <time className="shrink-0 text-muted-foreground">{l.time}</time>
            <span className={color(l.kind)}>{l.text}</span>
          </div>
        ))}
      </Card>
    </>
  );
}
