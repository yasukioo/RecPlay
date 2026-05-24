import { echarts, ReactEChartsCore } from "../../charts/echarts";
import { getTranslations } from "../../i18n";
import { getMonitorDashboardClasses } from "../../layoutModel";
import { useLocaleStore } from "../../stores/localeStore";
import { useStatsStore } from "../../stores/statsStore";
import {
  buildLatencyDistribution,
  buildSecondaryTrends,
  getRingBufferPercent,
} from "./monitorDashboardModel";

export function MonitorDashboard() {
  const locale = useLocaleStore((s) => s.locale);
  const stats = useStatsStore((s) => s);
  const layoutClasses = getMonitorDashboardClasses();
  const t = getTranslations(locale);

  const ringBufferPct = getRingBufferPercent(stats.ringbuf_used, stats.ringbuf_capacity);
  const latencyDistribution = buildLatencyDistribution(stats.write_latency_p99_ms);
  const secondaryTrends = buildSecondaryTrends(stats.history, locale);

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

  const ringGaugeOption = {
    series: [
      {
        type: "gauge",
        startAngle: 210,
        endAngle: -30,
        min: 0,
        max: 100,
        progress: { show: true, width: 10 },
        pointer: { show: false },
        axisLine: { lineStyle: { width: 10, color: [[1, "#2a2f38"]] } },
        axisTick: { show: false },
        splitLine: { show: false },
        axisLabel: { show: false },
        detail: {
          valueAnimation: false,
          formatter: "{value}%",
          color: "#e8eaed",
          fontSize: 18,
          offsetCenter: [0, "10%"],
        },
        title: {
          color: "#9aa0a8",
          fontSize: 11,
          offsetCenter: [0, "72%"],
        },
        data: [{ value: ringBufferPct, name: t.monitorDashboard.ringBuffer }],
      },
    ],
  };

  const latencyOption = {
    grid: { top: 16, bottom: 8, left: 56, right: 16 },
    xAxis: {
      type: "value" as const,
      max: 100,
      axisLabel: { fontSize: 10, color: "#9aa0a8", formatter: "{value}%" },
      splitLine: { lineStyle: { color: "#2a2f38" } },
    },
    yAxis: {
      type: "category" as const,
      data: latencyDistribution.map((item) => item.label),
      axisLabel: { fontSize: 10, color: "#9aa0a8" },
      axisTick: { show: false },
      axisLine: { show: false },
    },
    series: [
      {
        type: "bar",
        data: latencyDistribution.map((item) => item.value),
        barWidth: 12,
        itemStyle: {
          borderRadius: 999,
          color: (params: { dataIndex: number }) => ["#66bb6a", "#ffa726", "#ef5350"][params.dataIndex] ?? "#4fc3f7",
        },
        label: {
          show: true,
          position: "right" as const,
          color: "#e8eaed",
          formatter: "{c}%",
        },
      },
    ],
    tooltip: { show: false },
  };

  return (
    <div className="space-y-3">
      <div className={layoutClasses.metricGrid}>
        <MetricCard label={t.monitorDashboard.throughput} value={`${stats.total_throughput_mbps.toFixed(2)} Mbps`} />
        <MetricCard label={t.monitorDashboard.packets} value={stats.total_packets.toLocaleString()} />
        <MetricCard
          label={t.monitorDashboard.drops}
          value={stats.total_drops.toLocaleString()}
          danger={stats.total_drops > 0}
        />
        <MetricCard label={t.monitorDashboard.p99Latency} value={`${stats.write_latency_p99_ms.toFixed(2)} ms`} />
        <MetricCard label={t.monitorDashboard.ringBuffer} value={`${ringBufferPct}%`} danger={ringBufferPct > 80} />
        <MetricCard label={t.monitorDashboard.diskQueue} value={`${stats.disk_queue_bytes.toLocaleString()} B`} />
      </div>

      <div className={layoutClasses.chartGrid}>
        <div className="rounded border border-hmi-border bg-hmi-surface p-2">
          <div className="mb-2 text-xs uppercase tracking-wide text-hmi-text-muted">{t.monitorDashboard.throughput}</div>
          <ReactEChartsCore
            echarts={echarts}
            option={throughputOption}
            style={{ height: "160px" }}
            opts={{ renderer: "svg" }}
          />
        </div>
        <div className="rounded border border-hmi-border bg-hmi-surface p-2">
          <div className="mb-2 text-xs uppercase tracking-wide text-hmi-text-muted">{t.monitorDashboard.ringBuffer}</div>
          <ReactEChartsCore
            echarts={echarts}
            option={ringGaugeOption}
            style={{ height: "160px" }}
            opts={{ renderer: "svg" }}
          />
        </div>
        <div className="rounded border border-hmi-border bg-hmi-surface p-2">
          <div className="mb-2 text-xs uppercase tracking-wide text-hmi-text-muted">{t.monitorDashboard.latencyMix}</div>
          <ReactEChartsCore
            echarts={echarts}
            option={latencyOption}
            style={{ height: "160px" }}
            opts={{ renderer: "svg" }}
          />
        </div>
      </div>

      <div className={layoutClasses.trendGrid}>
        {secondaryTrends.map((trend) => {
          const option = {
            grid: { top: 8, bottom: 8, left: 8, right: 8 },
            xAxis: {
              type: "category" as const,
              data: trend.data.map((_, index) => index),
              show: false,
            },
            yAxis: {
              type: "value" as const,
              show: false,
            },
            series: [
              {
                type: "line",
                data: trend.data,
                smooth: true,
                symbol: "none",
                areaStyle: { color: `${trend.color}22` },
                lineStyle: { color: trend.color, width: 1.8 },
              },
            ],
            tooltip: { show: false },
          };

          return (
            <div key={trend.label} className="rounded border border-hmi-border bg-hmi-surface p-2">
              <div className="text-xs uppercase tracking-wide text-hmi-text-muted">{trend.label}</div>
              <div className="mt-1 font-mono text-base font-bold text-hmi-text">{trend.value}</div>
              <ReactEChartsCore
                echarts={echarts}
                option={option}
                style={{ height: "72px" }}
                opts={{ renderer: "svg" }}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
}

function MetricCard({ label, value, danger }: { label: string; value: string; danger?: boolean }) {
  return (
    <div
      className={`rounded border p-3 ${
        danger ? "border-hmi-danger/30 bg-hmi-danger/5" : "border-hmi-border bg-hmi-surface"
      }`}
    >
      <div className="text-xs text-hmi-text-muted">{label}</div>
      <div className="mt-1 font-mono text-lg font-bold">{value}</div>
    </div>
  );
}
