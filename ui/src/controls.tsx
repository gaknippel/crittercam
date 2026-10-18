// Setting rows and key capture, built on shadcn/ui.
import { useEffect, useState } from "react";
import { Binding, FloatSetting, keyLabel, vkFromCode } from "./protocol";
import { cn } from "@/lib/utils";
import { Slider } from "@/components/ui/slider";
import { Switch } from "@/components/ui/switch";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import { ToggleGroup, ToggleGroupItem } from "@/components/ui/toggle-group";

const toPosition = (d: FloatSetting, v: number) =>
  d.log ? Math.log(v / d.min) / Math.log(d.max / d.min) : (v - d.min) / (d.max - d.min);
const fromPosition = (d: FloatSetting, p: number) => {
  const v = d.log ? d.min * Math.pow(d.max / d.min, p) : d.min + p * (d.max - d.min);
  return Number(Math.min(d.max, Math.max(d.min, v)).toFixed(d.decimals));
};

// The game speed bar inside the speed notification: glides to every change.
function SpeedBar({ def, value, className }: { def: FloatSetting; value: number; className?: string }) {
  const at = (v: number) => `${toPosition(def, Math.min(def.max, Math.max(def.min, v))) * 100}%`;
  return (
    <div className={cn("border p-3", className)}>
      <div className="mb-2 flex items-baseline justify-between">
        <span className="text-xs text-muted-foreground">game speed</span>
        <span className="text-sm font-semibold tabular-nums">{value.toFixed(2)}×</span>
      </div>
      <div className="relative h-1.5 bg-muted">
        <div className="absolute inset-y-0 left-0 bg-primary transition-[width] duration-300 ease-out motion-reduce:transition-none" style={{ width: at(value) }} />
        <div className="absolute -inset-y-1 w-px bg-foreground/50" style={{ left: at(1) }} />
      </div>
      <div className="relative mt-1.5 h-3 text-[10px] text-muted-foreground">
        <span className="absolute left-0">{def.min}×</span>
        <span className="absolute -translate-x-1/2" style={{ left: at(1) }}>1×</span>
        <span className="absolute right-0">{def.max}×</span>
      </div>
    </div>
  );
}

// Notification that pops up whenever the live game speed changes, and fades out 1.6s after it stops changing.
export function SpeedNotification({ def, speed }: { def: FloatSetting; speed?: number }) {
  const [visible, setVisible] = useState(false);
  const [last, setLast] = useState(speed);
  if (speed !== last) {
    setLast(speed);
    if (speed !== undefined && last !== undefined) setVisible(true);
  }
  useEffect(() => {
    if (!visible) return;
    const timer = setTimeout(() => setVisible(false), 1600);
    return () => clearTimeout(timer);
  }, [visible, speed]);
  return (
    <SpeedBar
      def={def} value={speed ?? 1}
      className={cn(
        "pointer-events-none fixed bottom-5 left-1/2 w-72 -translate-x-1/2 bg-popover text-popover-foreground shadow-lg transition-all duration-300 ease-out motion-reduce:transition-none",
        visible ? "translate-y-0 opacity-100" : "translate-y-4 opacity-0",
      )}
    />
  );
}

export function Row({ label, hint, children }: { label: string; hint?: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between gap-6 py-3">
      <div className="min-w-0">
        <div className="text-sm">{label}</div>
        {hint && <div className="text-xs text-muted-foreground">{hint}</div>}
      </div>
      {children}
    </div>
  );
}

export function SliderRow({ def, value, onChange }: { def: FloatSetting; value: number; onChange: (v: number) => void }) {
  const [text, setText] = useState(value.toFixed(def.decimals));
  useEffect(() => setText(value.toFixed(def.decimals)), [value, def.decimals]);
  const commit = () => {
    const v = parseFloat(text);
    if (Number.isFinite(v)) onChange(Number(Math.min(def.max, Math.max(def.min, v)).toFixed(def.decimals)));
    else setText(value.toFixed(def.decimals));
  };
  return (
    <Row label={def.label} hint={def.hint}>
      <div className="flex w-80 shrink-0 items-center gap-3">
        <Slider
          min={0} max={1000} step={1} aria-label={def.label}
          value={[Math.round(toPosition(def, value) * 1000)]}
          onValueChange={([p]) => onChange(fromPosition(def, p / 1000))}
          onDoubleClick={() => onChange(def.fallback)}
          title="double-click to reset"
        />
        <Input
          className="h-7 w-20 text-right tabular-nums" value={text} aria-label={`${def.label} value`}
          onChange={(e) => setText(e.target.value)} onBlur={commit}
          onKeyDown={(e) => e.key === "Enter" && (e.target as HTMLInputElement).blur()}
        />
      </div>
    </Row>
  );
}

export function SwitchRow({ label, hint, value, onChange }: { label: string; hint?: string; value: boolean; onChange: (v: boolean) => void }) {
  return (
    <Row label={label} hint={hint}>
      <Switch checked={value} onCheckedChange={onChange} aria-label={label} />
    </Row>
  );
}

export function ChoiceRow({ label, hint, options, value, onChange }: { label: string; hint?: string; options: string[]; value: number; onChange: (v: number) => void }) {
  return (
    <Row label={label} hint={hint}>
      <ToggleGroup type="single" variant="outline" size="sm" value={String(value)} onValueChange={(v) => v && onChange(Number(v))} aria-label={label}>
        {options.map((o, i) => <ToggleGroupItem key={o} value={String(i)}>{o}</ToggleGroupItem>)}
      </ToggleGroup>
    </Row>
  );
}

export function Kbd({ children }: { children: React.ReactNode }) {
  return <kbd className="border bg-muted px-1.5 py-0.5 text-xs text-foreground">{children}</kbd>;
}

export function Keys({ binding }: { binding: Binding }) {
  return (
    <span className="inline-flex gap-1">
      {binding.ctrl && <Kbd>ctrl</Kbd>}
      {binding.alt && <Kbd>alt</Kbd>}
      {binding.shift && <Kbd>shift</Kbd>}
      <Kbd>{keyLabel(binding.vk)}</Kbd>
    </span>
  );
}

const MODIFIER_CODES = new Set(["ShiftLeft", "ShiftRight", "ControlLeft", "ControlRight", "AltLeft", "AltRight", "MetaLeft", "MetaRight"]);

export function KeyCapture({ binding, onChange, conflict }: { binding: Binding; onChange: (b: Binding) => void; conflict?: string }) {
  const [listening, setListening] = useState(false);
  useEffect(() => {
    if (!listening) return;
    const onKey = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (e.code === "Escape") return setListening(false);
      if (MODIFIER_CODES.has(e.code)) return;
      const vk = vkFromCode(e.code);
      if (vk === undefined) return;
      onChange({ vk, alt: e.altKey, ctrl: e.ctrlKey, shift: e.shiftKey });
      setListening(false);
    };
    window.addEventListener("keydown", onKey, true);
    return () => window.removeEventListener("keydown", onKey, true);
  }, [listening, onChange]);
  return (
    <div className="flex items-center gap-2">
      {conflict && <span className="text-xs text-destructive" title={`also bound to: ${conflict}`}>clash</span>}
      <Button
        variant={listening ? "default" : "ghost"} size="sm"
        onClick={() => setListening(!listening)} onBlur={() => setListening(false)}
        title={conflict ? `also bound to: ${conflict}` : "click, then press a key. esc cancels."}
      >
        {listening ? "press a key…" : <Keys binding={binding} />}
      </Button>
    </div>
  );
}
