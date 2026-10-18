// "studio" page: play gestures on your own character and float it off the ground, for greenscreen recordings.
import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Switch } from "@/components/ui/switch";
import { Card } from "@/components/ui/card";
import { GESTURES, IDLE_EVENT } from "./protocol";
import { Row } from "./controls";

type Studio = { play: (eventName: string) => void; float: (on: boolean) => void; invincible: (on: boolean) => void; enemiesPassive: (on: boolean) => void; lift: (meters: number) => void };

export function StudioPage({ studio, connected }: { studio: Studio; connected: boolean }) {
  const [floating, setFloating] = useState(false);
  const [custom, setCustom] = useState("W_Event80010");
  const [last, setLast] = useState<number | null>(null);
  const [invincible, setInvincible] = useState(false);
  const [passive, setPassive] = useState(false);
  if (!connected) return <p className="text-sm text-muted-foreground">inject the camera to use the studio.</p>;
  const toggleFloat = (on: boolean) => {
    setFloating(on);
    studio.float(on);
  };
  // the dll plays each gesture once, then back to standing (sitting ones stay seated until stop)
  const play = studio.play;
  const stop = () => {
    studio.play(IDLE_EVENT);
    setLast(null);
  };
  const lift = (m: number) => {
    setFloating(true); // lifting turns floating on in the dll too
    studio.lift(m);
  };
  return (
    <>
      <Card className="mb-4 gap-0 py-0">
        <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">character</div>
        <div className="divide-y px-4">
          <Row label="no gravity" hint="your character stays where it is, gestures still play">
            <Switch checked={floating} onCheckedChange={toggleFloat} aria-label="no gravity" />
          </Row>
          <Row label="invincible" hint="hits still land, but your hp stays full">
            <Switch checked={invincible} onCheckedChange={(on) => { setInvincible(on); studio.invincible(on); }} aria-label="invincible" />
          </Row>
          <Row label="enemies ignore you" hint="they stay alive and moving, but don't target you. their hits do nothing">
            <Switch checked={passive} onCheckedChange={(on) => { setPassive(on); studio.enemiesPassive(on); }} aria-label="enemies ignore you" />
          </Row>
          <Row label="move up / down" hint="lift off the floor so a depth key can't catch it">
            <div className="flex gap-1">
              {[-1, -0.25, 0.25, 1, 5].map((m) => (
                <Button key={m} size="sm" variant="outline" onClick={() => lift(m)}>{m > 0 ? `+${m}` : m} m</Button>
              ))}
            </div>
          </Row>
        </div>
      </Card>

      <Card className="mb-4 gap-0 py-0">
        <div className="flex items-center justify-between gap-4 border-b px-4 py-2 text-xs text-muted-foreground">
          <span className="font-medium">gestures ({GESTURES.length})</span>
          <Button size="sm" variant="outline" onClick={stop}>stop</Button>
        </div>
        <div className="grid grid-cols-[repeat(auto-fill,minmax(150px,1fr))] gap-1 p-4">
          {GESTURES.map(([id, name]) => (
            <Button key={id} size="sm" variant={last === id ? "secondary" : "outline"} className="h-auto min-h-7 justify-start py-1 text-left whitespace-normal" title={`W_Event${id}`} onClick={() => { setLast(id); play(`W_Event${id}`); }}>
              {name}
            </Button>
          ))}
        </div>
      </Card>

      <Card className="gap-0 py-0">
        <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">any animation</div>
        <form className="flex gap-2 p-4" onSubmit={(e) => { e.preventDefault(); play(custom.trim()); }}>
          <Input value={custom} onChange={(e) => setCustom(e.target.value)} aria-label="behavior event name" className="normal-case" spellCheck={false} />
          <Button type="submit" variant="outline">play</Button>
        </form>
        <p className="px-4 pb-4 text-xs text-muted-foreground">behavior event names, like w_event80010. unknown names show a notification.</p>
      </Card>
    </>
  );
}
