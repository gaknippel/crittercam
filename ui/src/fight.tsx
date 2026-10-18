// "fight" page: start any fight from the Nightreign Boss Arena mod with one click (sets the flag its menu would set),
// plus a raw event flag box for experiments.
import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Card } from "@/components/ui/card";
import { FIGHTS } from "./fights";

type Flags = { set: (flag: number, on: boolean) => void; get: (flag: number) => void };

const CATEGORIES = [...new Set(FIGHTS.map((f) => f.category))];

export function FightPage({ flags, connected }: { flags: Flags; connected: boolean }) {
  const [search, setSearch] = useState("");
  const [custom, setCustom] = useState("12005150");
  if (!connected) return <p className="text-sm text-muted-foreground">inject the camera to start fights.</p>;
  const shown = FIGHTS.filter((f) => f.name.includes(search.trim().toLowerCase()));
  const id = Number(custom);
  return (
    <>
      <p className="mb-4 text-xs text-muted-foreground">
        needs the nightreign boss arena mod (launch the game with its me3 profile) and you in the roundtable hold. pick a boss and the mod warps you into its arena.
      </p>
      <Input value={search} onChange={(e) => setSearch(e.target.value)} placeholder="search bosses" aria-label="search bosses" className="mb-4" />
      {CATEGORIES.map((category) => {
        const list = shown.filter((f) => f.category === category);
        if (list.length === 0) return null;
        return (
          <Card key={category} className="mb-4 gap-0 py-0">
            <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">{category} ({list.length})</div>
            <div className="divide-y px-4">
              {list.map((f) => (
                <div key={f.trigger} className="flex items-center justify-between gap-4 py-1.5 text-sm">
                  <span>{f.name}</span>
                  <Button size="sm" variant="outline" onClick={() => flags.set(f.trigger, true)}>fight</Button>
                </div>
              ))}
            </div>
          </Card>
        );
      })}
      <Card className="gap-0 py-0">
        <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">any event flag</div>
        <div className="flex items-center gap-2 p-4">
          <Input value={custom} onChange={(e) => setCustom(e.target.value.replace(/\D/g, ""))} aria-label="event flag id" className="tabular-nums" />
          {custom !== "" && (
            <span className="flex gap-1">
              <Button size="sm" variant="outline" onClick={() => flags.get(id)}>check</Button>
              <Button size="sm" variant="outline" onClick={() => flags.set(id, true)}>on</Button>
              <Button size="sm" variant="outline" onClick={() => flags.set(id, false)}>off</Button>
            </span>
          )}
        </div>
      </Card>
    </>
  );
}
