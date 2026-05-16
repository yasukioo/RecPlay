import { useStatsStore } from "../../stores/statsStore";

interface StatusBarProps {
  wsStatus: "connecting" | "open" | "closed" | "error";
}

export function StatusBar({ wsStatus }: StatusBarProps) {
  const throughput = useStatsStore((s) => s.total_throughput_mbps);
  const drops = useStatsStore((s) => s.total_drops);

  const wsColor: Record<string, string> = {
    open: "text-hmi-success",
    connecting: "text-hmi-warning",
    closed: "text-hmi-text-muted",
    error: "text-hmi-danger",
  };

  return (
    <footer className="flex items-center justify-between px-4 py-1 bg-hmi-surface border-t border-hmi-border text-xs">
      <div className="flex items-center gap-4">
        <span className={wsColor[wsStatus]}>
          WS: {wsStatus}
        </span>
        <span className="text-hmi-text-muted">
          Throughput: {throughput.toFixed(2)} Mbps
        </span>
        {drops > 0 && (
          <span className="text-hmi-danger">
            Drops: {drops.toLocaleString()}
          </span>
        )}
      </div>
      <div className="text-hmi-text-muted">
        {/* TODO: Codex — add clock, CPU usage, disk space indicators */}
        RecPlay Console
      </div>
    </footer>
  );
}
