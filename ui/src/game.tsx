// "in game" tab: live camera numbers, the dolly nodes, and a top-down map of the path. Data comes from the dll ~10x a second.
import { useState } from "react";
import { Card } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { GameState } from "./protocol";

const W = 640, H = 340, PAD = 28;
const CAM = "text-orange-600 dark:text-orange-400"; // the camera
const ME = "text-blue-600 dark:text-blue-400"; // you and other players; enemies stay gray

const f = (n: number, d = 2) => n.toFixed(d);

// grid step that gives roughly 4-8 lines across the visible range
function niceStep(range: number) {
  const raw = range / 6, pow = Math.pow(10, Math.floor(Math.log10(raw)));
  return [1, 2, 5, 10].map((m) => m * pow).find((s) => s >= raw) ?? 10 * pow;
}

function PathMap({ game }: { game: GameState }) {
  const [hover, setHover] = useState<number | null>(null);
  // fit the path, the camera and you; other characters are only drawn when they fall inside that view
  const xz = [...game.curve, ...game.nodes, ...(game.cam ? [game.cam] : []), ...(game.me ? [game.me] : [])].map((p) => [p[0], p[2]]);
  if (xz.length === 0) {
    return <p className="p-4 text-sm text-muted-foreground">nothing to show yet. turn the camera on or add a node.</p>;
  }
  const xs = xz.map((p) => p[0]), zs = xz.map((p) => p[1]);
  const [minX, maxX, minZ, maxZ] = [Math.min(...xs), Math.max(...xs), Math.min(...zs), Math.max(...zs)];
  // same scale on both axes so distances and angles are true; at least 4m across so a single node doesn't zoom in forever
  const scale = Math.min((W - 2 * PAD) / Math.max(maxX - minX, 4), (H - 2 * PAD) / Math.max(maxZ - minZ, 4));
  const cx = (minX + maxX) / 2, cz = (minZ + maxZ) / 2;
  // top down: +x right, +z (forward at yaw 0) up
  const sx = (x: number) => W / 2 + (x - cx) * scale;
  const sy = (z: number) => H / 2 - (z - cz) * scale;
  const dir = (yaw: number, len: number) => [Math.sin((yaw * Math.PI) / 180) * len, -Math.cos((yaw * Math.PI) / 180) * len];

  const step = niceStep(Math.max(W, H) / scale);
  const gridX: number[] = [], gridZ: number[] = [];
  for (let x = Math.ceil((cx - W / 2 / scale) / step) * step; x <= cx + W / 2 / scale; x += step) gridX.push(x);
  for (let z = Math.ceil((cz - H / 2 / scale) / step) * step; z <= cz + H / 2 / scale; z += step) gridZ.push(z);

  const hovered = hover !== null ? game.nodes[hover] : null;
  return (
    <div className="relative">
      <svg viewBox={`0 0 ${W} ${H}`} className="block w-full" role="img" aria-label={`top-down map of ${game.nodes.length} dolly nodes${game.cam ? " and the camera" : ""}`}>
        <g className="stroke-border" strokeWidth={1}>
          {gridX.map((x) => <line key={`x${x}`} x1={sx(x)} x2={sx(x)} y1={0} y2={H} />)}
          {gridZ.map((z) => <line key={`z${z}`} y1={sy(z)} y2={sy(z)} x1={0} x2={W} />)}
        </g>
        {(game.chrs ?? []).map((c, i) => (
          <rect key={`c${i}`} className={c[3] ? `fill-current ${ME} opacity-50` : "fill-muted-foreground opacity-60"} x={sx(c[0]) - 3} y={sy(c[2]) - 3} width={6} height={6} />
        ))}
        {game.me && <rect className={`fill-current stroke-background ${ME}`} strokeWidth={2} x={sx(game.me[0]) - 6} y={sy(game.me[2]) - 6} width={12} height={12} />}
        {game.curve.length > 1 && (
          <polyline className="stroke-foreground" fill="none" strokeWidth={2} strokeLinejoin="round" points={game.curve.map((p) => `${sx(p[0])},${sy(p[2])}`).join(" ")} />
        )}
        {game.nodes.map((n, i) => {
          const [dx, dy] = dir(n[3], 14);
          return (
            <g key={i} onMouseEnter={() => setHover(i)} onMouseLeave={() => setHover(null)}>
              <line className="stroke-muted-foreground" strokeWidth={1.5} x1={sx(n[0])} y1={sy(n[2])} x2={sx(n[0]) + dx} y2={sy(n[2]) + dy} />
              <rect className="fill-background stroke-foreground" strokeWidth={2} x={sx(n[0]) - 4} y={sy(n[2]) - 4} width={8} height={8} />
              <text className="fill-foreground text-[11px]" x={sx(n[0]) + 8} y={sy(n[2]) - 7}>{i + 1}</text>
              <rect fill="transparent" x={sx(n[0]) - 11} y={sy(n[2]) - 11} width={22} height={22} />
            </g>
          );
        })}
        {game.cam && (() => {
          const [x, y] = [sx(game.cam[0]), sy(game.cam[2])];
          const [dx, dy] = dir(game.cam[3], 22);
          const [lx, ly] = dir(game.cam[3] - 90, 6);
          return (
            <g className={CAM}>
              <line stroke="currentColor" strokeWidth={2} x1={x} y1={y} x2={x + dx} y2={y + dy} />
              <polygon fill="currentColor" className="stroke-background" strokeWidth={2} points={`${x + dx * 0.45},${y + dy * 0.45} ${x + lx},${y + ly} ${x - lx},${y - ly}`} />
            </g>
          );
        })()}
        <text className="fill-muted-foreground text-[11px]" x={8} y={H - 8}>grid {step} m · top down</text>
      </svg>
      {hovered && hover !== null && (
        <div
          className="pointer-events-none absolute border bg-popover px-2 py-1 text-xs text-popover-foreground shadow-md"
          style={{ left: `${(sx(hovered[0]) / W) * 100}%`, top: `${(sy(hovered[2]) / H) * 100}%`, transform: "translate(12px, 12px)" }}
        >
          <div className="font-medium">node {hover + 1}</div>
          <div className="text-muted-foreground">{f(hovered[0])}, {f(hovered[1])}, {f(hovered[2])}</div>
          <div className="text-muted-foreground">yaw {f(hovered[3], 0)}° · fov {f(hovered[5], 0)}°</div>
        </div>
      )}
    </div>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <div className="px-4 py-3">
      <div className="text-xs text-muted-foreground">{label}</div>
      <div className="text-sm tabular-nums">{value}</div>
    </div>
  );
}

export function GamePage({ game, connected }: { game: GameState | null; connected: boolean }) {
  if (!connected) {
    return <p className="text-sm text-muted-foreground">inject the camera to see live data from the game.</p>;
  }
  if (!game) {
    // the dll sends a snapshot every 100 ms; none at all means an old crittercam.dll without this feature is loaded.
    return <p className="text-sm text-muted-foreground">connected, but no data from the camera. the game is probably running an old crittercam.dll: close the game, rebuild the dll in visual studio, and inject again.</p>;
  }
  const cam = game.cam;
  return (
    <>
      <div className="mb-4 flex flex-wrap gap-2">
        <Badge variant={game.on ? "default" : "outline"}>camera {game.on ? "on" : "off"}</Badge>
        <Badge variant={game.paused ? "default" : "outline"}>{game.paused ? "paused" : game.speed !== undefined && game.speed !== 1 ? `speed ${game.speed.toFixed(2)}×` : "running"}</Badge>
        <Badge variant={game.playing ? "default" : "outline"}>dolly {game.playing ? "playing" : "stopped"}</Badge>
        <Badge variant="outline">hud {game.hud ? "visible" : "hidden"}</Badge>
        {game.loop ? <Badge variant="outline">loop</Badge> : null}
        {game.chrs && <Badge variant="outline">{game.chrs.filter((c) => !c[3]).length} enemies · {game.chrs.filter((c) => c[3]).length} players</Badge>}
      </div>

      <Card className="mb-4 gap-0 py-0">
        <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">camera</div>
        {cam ? (
          <div className="grid grid-cols-[2fr_1fr_1fr_1fr] divide-x">
            <Stat label="position" value={`${f(cam[0])}, ${f(cam[1])}, ${f(cam[2])}`} />
            <Stat label="yaw / pitch" value={`${f(cam[3], 1)}° / ${f(cam[4], 1)}°`} />
            <Stat label="roll" value={`${f(cam[5], 1)}°`} />
            <Stat label="fov" value={`${f(cam[6], 1)}°`} />
          </div>
        ) : (
          <p className="px-4 py-3 text-sm text-muted-foreground">turn the camera on in game to see where it is.</p>
        )}
      </Card>

      <Card className="mb-4 gap-0 py-0">
        <div className="flex items-center justify-between border-b px-4 py-3 text-xs text-muted-foreground">
          <span className="font-medium">path</span>
          <span className="flex items-center gap-4">
            <span className="flex items-center gap-1.5"><span className="h-0.5 w-4 bg-foreground" />curve</span>
            <span className="flex items-center gap-1.5"><span className="size-2 border-2 border-foreground" />node</span>
            <span className="flex items-center gap-1.5"><span className={`size-2 bg-current ${CAM}`} />camera</span>
            <span className="flex items-center gap-1.5"><span className={`size-2 bg-current ${ME}`} />you</span>
            <span className="flex items-center gap-1.5"><span className={`size-2 bg-current opacity-50 ${ME}`} />players</span>
            <span className="flex items-center gap-1.5"><span className="size-2 bg-muted-foreground opacity-60" />enemies</span>
          </span>
        </div>
        <PathMap game={game} />
      </Card>

      <Card className="gap-0 py-0">
        <div className="border-b px-4 py-3 text-xs font-medium text-muted-foreground">nodes ({game.nodes.length})</div>
        {game.nodes.length === 0 ? (
          <p className="px-4 py-3 text-sm text-muted-foreground">none yet.</p>
        ) : (
          <table className="w-full text-xs tabular-nums">
            <thead className="text-muted-foreground">
              <tr className="border-b">
                {["#", "x", "y", "z", "yaw", "pitch", "fov"].map((h) => <th key={h} className="px-4 py-2 text-right font-normal first:text-left">{h}</th>)}
              </tr>
            </thead>
            <tbody>
              {game.nodes.map((n, i) => (
                <tr key={i} className="border-b last:border-0 hover:bg-muted/50">
                  <td className="px-4 py-1.5">{i + 1}</td>
                  {[f(n[0]), f(n[1]), f(n[2]), `${f(n[3], 1)}°`, `${f(n[4], 1)}°`, `${f(n[5], 1)}°`].map((v, j) => <td key={j} className="px-4 py-1.5 text-right">{v}</td>)}
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Card>
    </>
  );
}
