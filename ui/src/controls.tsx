// Setting rows and key capture, built on shadcn/ui.
import { useEffect, useState } from "react";
import { Binding, FloatSetting, keyLabel, vkFromCode } from "./protocol";
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
