import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { MonitorDashboard } from "./MonitorDashboard";

const localeState = vi.hoisted(() => ({
  locale: "en-US" as "en-US" | "zh-CN",
}));

const statsState = vi.hoisted(() => ({
  total_throughput_mbps: 12.34,
  total_packets: 1234,
  total_drops: 5,
  drop_rate: 0.01,
  write_latency_p99_ms: 6.7,
  ringbuf_used: 32,
  ringbuf_capacity: 64,
  disk_queue_bytes: 2048,
  history: [
    {
      timestamp: 1,
      total_packets: 1000,
      total_drops: 3,
      total_throughput_mbps: 10,
      drop_rate: 0.01,
      write_latency_p99_ms: 5,
      ringbuf_used: 24,
      ringbuf_capacity: 64,
      disk_queue_bytes: 1024,
    },
    {
      timestamp: 2,
      total_packets: 1234,
      total_drops: 5,
      total_throughput_mbps: 12.34,
      drop_rate: 0.01,
      write_latency_p99_ms: 6.7,
      ringbuf_used: 32,
      ringbuf_capacity: 64,
      disk_queue_bytes: 2048,
    },
  ],
}));

vi.mock("../../charts/echarts", () => ({
  echarts: {},
  ReactEChartsCore: () => createElement("div", { "data-chart": "true" }),
}));

vi.mock("../../stores/localeStore", () => ({
  useLocaleStore: (
    selector: (state: {
      locale: typeof localeState.locale;
      setLocale: () => void;
      toggleLocale: () => void;
    }) => unknown,
  ) =>
    selector({
      locale: localeState.locale,
      setLocale: () => undefined,
      toggleLocale: () => undefined,
    }),
}));

vi.mock("../../stores/statsStore", () => ({
  useStatsStore: (
    selector: (state: typeof statsState & { replaceSnapshot: () => void }) => unknown,
  ) =>
    selector({
      ...statsState,
      replaceSnapshot: () => undefined,
    }),
}));

afterEach(() => {
  localeState.locale = "en-US";
});

describe("MonitorDashboard", () => {
  it("renders Simplified Chinese labels instead of hard-coded English metric titles", () => {
    localeState.locale = "zh-CN";

    const html = renderToStaticMarkup(createElement(MonitorDashboard));

    expect(html).toContain("吞吐");
    expect(html).toContain("报文数");
    expect(html).toContain("丢包");
    expect(html).toContain("P99 延迟");
    expect(html).toContain("环形缓冲");
    expect(html).toContain("磁盘队列");
    expect(html).toContain("延迟分布");
    expect(html).not.toContain(">Throughput<");
    expect(html).not.toContain(">Packets<");
    expect(html).not.toContain(">Ring Buffer<");
    expect(html).not.toContain(">Latency Mix<");
  });
});
