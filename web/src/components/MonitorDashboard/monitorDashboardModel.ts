import { getTranslations } from "../../i18n";
import type { Locale, StatsHistoryPoint } from "../../types";

export interface SecondaryTrend {
  label: string;
  value: string;
  data: number[];
  color: string;
}

export function getRingBufferPercent(used: number, capacity: number): number {
  if (capacity <= 0) {
    return 0;
  }

  return Math.min(100, Math.max(0, Math.round((used / capacity) * 100)));
}

export function buildLatencyDistribution(p99LatencyMs: number): Array<{ label: string; value: number }> {
  if (p99LatencyMs <= 5) {
    return [
      { label: "Low", value: 85 },
      { label: "Medium", value: 15 },
      { label: "High", value: 0 },
    ];
  }

  if (p99LatencyMs <= 10) {
    return [
      { label: "Low", value: 45 },
      { label: "Medium", value: 45 },
      { label: "High", value: 10 },
    ];
  }

  return [
    { label: "Low", value: 25 },
    { label: "Medium", value: 50 },
    { label: "High", value: 25 },
  ];
}

function formatBytes(value: number): string {
  if (value < 1024) {
    return `${value} B`;
  }

  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }

  return `${(value / (1024 * 1024)).toFixed(1)} MB`;
}

export function buildSecondaryTrends(history: StatsHistoryPoint[], locale: Locale = "en-US"): SecondaryTrend[] {
  const last = history.length > 0 ? history[history.length - 1] : undefined;
  const t = getTranslations(locale);

  return [
    {
      label: t.monitorDashboard.trendLabels.packets,
      value: (last?.total_packets ?? 0).toLocaleString(),
      data: history.map((point) => point.total_packets),
      color: "#4fc3f7",
    },
    {
      label: t.monitorDashboard.trendLabels.drops,
      value: (last?.total_drops ?? 0).toLocaleString(),
      data: history.map((point) => point.total_drops),
      color: "#ef5350",
    },
    {
      label: t.monitorDashboard.trendLabels.latency,
      value: `${(last?.write_latency_p99_ms ?? 0).toFixed(1)} ms`,
      data: history.map((point) => point.write_latency_p99_ms ?? 0),
      color: "#ffa726",
    },
    {
      label: t.monitorDashboard.trendLabels.diskQueue,
      value: formatBytes(last?.disk_queue_bytes ?? 0),
      data: history.map((point) => point.disk_queue_bytes ?? 0),
      color: "#66bb6a",
    },
  ];
}
