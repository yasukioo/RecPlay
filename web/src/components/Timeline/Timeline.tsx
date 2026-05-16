import ReactECharts from "echarts-for-react";
import { useSessionStore } from "../../stores/sessionStore";
import { useStatsStore } from "../../stores/statsStore";

export function Timeline() {
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const history = useStatsStore((s) => s.history);

  // TODO: Codex — implement interactive playhead dragging, zoom, marker display
  const option = {
    grid: { top: 8, bottom: 20, left: 40, right: 16 },
    xAxis: {
      type: "category" as const,
      data: history.map((_, i) => i),
      show: false,
    },
    yAxis: {
      type: "value" as const,
      show: false,
    },
    series: [
      {
        type: "line",
        data: history.map((p) => p.total_throughput_mbps),
        smooth: true,
        symbol: "none",
        areaStyle: { color: "rgba(79,195,247,0.2)" },
        lineStyle: { color: "#4fc3f7", width: 1.5 },
      },
    ],
    tooltip: { show: false },
  };

  const progressPct = durationNs > 0 ? (positionNs / durationNs) * 100 : 0;

  return (
    <div className="relative bg-hmi-surface rounded border border-hmi-border p-2">
      {/* Sparkline chart */}
      <ReactECharts option={option} style={{ height: "64px" }} opts={{ renderer: "svg" }} />

      {/* Playhead indicator */}
      <div
        className="absolute top-0 bottom-0 w-px bg-hmi-accent pointer-events-none"
        style={{ left: `${progressPct}%` }}
      />

      {/* TODO: Codex — add timeline markers, loop region overlay, click-to-seek */}
    </div>
  );
}
