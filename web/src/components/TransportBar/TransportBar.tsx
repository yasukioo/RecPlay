import { useSessionStore } from "../../stores/sessionStore";

export function TransportBar() {
  const state = useSessionStore((s) => s.state);
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const speed = useSessionStore((s) => s.speed);

  const formatTime = (ns: number) => {
    const sec = ns / 1_000_000_000;
    const m = Math.floor(sec / 60);
    const s = (sec % 60).toFixed(1);
    return `${m}:${s.padStart(4, "0")}`;
  };

  // TODO: Codex — wire up transport button click handlers to API calls
  const handleRecord = () => console.log("transport: record");
  const handlePlay = () => console.log("transport: play");
  const handlePause = () => console.log("transport: pause");
  const handleStop = () => console.log("transport: stop");

  return (
    <div className="flex items-center gap-4 px-4 py-2 bg-hmi-surface border-t border-b border-hmi-border">
      {/* Transport buttons */}
      <div className="flex items-center gap-2">
        <button
          type="button"
          onClick={handleRecord}
          className="w-8 h-8 rounded-full bg-hmi-danger/20 border border-hmi-danger/50 text-hmi-danger hover:bg-hmi-danger/30 text-xs"
          title="Record"
        >
          ●
        </button>
        <button
          type="button"
          onClick={handlePlay}
          className="w-8 h-8 rounded bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border text-xs"
          title="Play"
        >
          ▶
        </button>
        <button
          type="button"
          onClick={handlePause}
          className="w-8 h-8 rounded bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border text-xs"
          title="Pause"
        >
          ⏸
        </button>
        <button
          type="button"
          onClick={handleStop}
          className="w-8 h-8 rounded bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border text-xs"
          title="Stop"
        >
          ■
        </button>
      </div>

      {/* Time display */}
      <div className="flex items-center gap-2 text-sm font-mono">
        <span className="text-hmi-accent">{formatTime(positionNs)}</span>
        <span className="text-hmi-text-muted">/</span>
        <span className="text-hmi-text-muted">{formatTime(durationNs)}</span>
      </div>

      {/* Progress bar */}
      <div className="flex-1 h-2 bg-hmi-surface-alt rounded-full overflow-hidden">
        {/* TODO: Codex — make this seekable (click + drag) */}
        <div
          className="h-full bg-hmi-accent transition-all"
          style={{ width: durationNs > 0 ? `${(positionNs / durationNs) * 100}%` : "0%" }}
        />
      </div>

      {/* Speed + State */}
      <span className="text-xs text-hmi-text-muted">{speed.toFixed(1)}x</span>
      <span className="text-xs px-2 py-0.5 rounded bg-hmi-surface-alt border border-hmi-border text-hmi-accent">
        {state}
      </span>
    </div>
  );
}
