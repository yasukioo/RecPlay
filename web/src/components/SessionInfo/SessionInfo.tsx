import { useSessionStore } from "../../stores/sessionStore";

export function SessionInfo() {
  const state = useSessionStore((s) => s.state);
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const speed = useSessionStore((s) => s.speed);
  const lastTransition = useSessionStore((s) => s.lastTransition);

  const formatNs = (ns: number) => `${(ns / 1_000_000_000).toFixed(2)}s`;

  return (
    <aside className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-4 overflow-y-auto">
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">Session</h3>

      <dl className="space-y-2 text-sm">
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">State</dt>
          <dd className="font-mono">{state}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">Position</dt>
          <dd className="font-mono">{formatNs(positionNs)}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">Duration</dt>
          <dd className="font-mono">{formatNs(durationNs)}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">Speed</dt>
          <dd className="font-mono">{speed.toFixed(2)}x</dd>
        </div>
      </dl>

      {lastTransition && (
        <div className="p-2 rounded bg-hmi-surface-alt border border-hmi-border text-xs">
          <span className="text-hmi-text-muted">Last: </span>
          <span>{lastTransition.old_state} → {lastTransition.new_state}</span>
        </div>
      )}

      {/* TODO: Codex — add record/playback config display, file info, loop region */}
    </aside>
  );
}
