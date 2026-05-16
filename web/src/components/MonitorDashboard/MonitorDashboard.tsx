import ReactECharts from "echarts-for-react";
import { useStatsStore } from "../../stores/statsStore";

export function MonitorDashboard() {
  const stats = useStatsStore((s) => s);

  const ringBufferPct =
    stats.ringbuf_capacity > 0
      ? Math.round((stats.ringbuf_used / stats.ringbuf_capacity) * 100)
      : 0;

  // TODO: Codex — add more chart types (gauge for ring buffer, bar for latency distribution)
  const throughputOption = {
    grid: { top: 16, bottom: 24, left: 48, right: 16 },
    xAxis: {
      type: "category" as const,
      data: stats.history.map((_, i) => i),
      axisLabel: { show: false },
      axisLine: { lineStyle: { color: "#3a3f4a" } },
    },
    yAxis: {
      type: "value" as const,
      axisLabel: { fontSize: 10, color: "#9aa0a8" },
      splitLine: { lineStyle: { color: "#2a2f38" } },
    },
    series: [
      {
        type: "line",
        data: stats.history.map((p) => p.total_throughput_mbps),
        smooth: true,
        symbol: "none",
        areaStyle: { color: "rgba(79,195,247,0.15)" },
        lineStyle: { color: "#4fc3f7", width: 2 },
      },
    ],
    tooltip: {
      trigger: "axis" as const,
      formatter: (params: Array<{ value: number }>) =>
        `${(params[0]?.value ?? 0).toFixed(2)} Mbps`,
    },
  };

  return (
    <div className="space-y-3">
      {/* Metric cards */}
      <div className="grid grid-cols-3 gap-2">
        <MetricCard label="Throughput" value={`${stats.total_throughput_mbps.toFixed(2)} Mbps`} />
        <MetricCard label="Packets" value={stats.total_packets.toLocaleString()} />
        <MetricCard label="Drops" value={stats.total_drops.toLocaleString()} danger={stats.total_drops > 0} />
        <MetricCard label="P99 Latency" value={`${stats.write_latency_p99_ms.toFixed(2)} ms`} />
        <MetricCard label="Ring Buffer" value={`${ringBufferPct}%`} danger={ringBufferPct > 80} />
        <MetricCard label="Disk Queue" value={`${stats.disk_queue_bytes.toLocaleString()} B`} />
      </div>

      {/* Throughput chart */}
      <div className="bg-hmi-surface rounded border border-hmi-border p-2">
        <ReactECharts option={throughputOption} style={{ height: "160px" }} opts={{ renderer: "svg" }} />
      </div>
    </div>
  );
}

function MetricCard({ label, value, danger }: { label: string; value: string; danger?: boolean }) {
  return (
    <div className={`p-3 rounded border ${
      danger ? "border-hmi-danger/30 bg-hmi-danger/5" : "border-hmi-border bg-hmi-surface"
    }`}>
      <div className="text-xs text-hmi-text-muted">{label}</div>
      <div className="text-lg font-bold font-mono mt-1">{value}</div>
    </div>
  );
}
