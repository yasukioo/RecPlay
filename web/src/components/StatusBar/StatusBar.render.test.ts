import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { describe, expect, it, vi } from "vitest";

import { StatusBar } from "./StatusBar";

const statsState = vi.hoisted(() => ({
  total_throughput_mbps: 12.34,
  total_packets: 1234,
  total_drops: 0,
  drop_rate: 0,
  write_latency_p99_ms: 0,
  ringbuf_used: 0,
  ringbuf_capacity: 64,
  disk_queue_bytes: 0,
  cpu_usage_percent: 37.5,
  history: [],
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

describe("StatusBar", () => {
  it("renders the actual cpu usage metric instead of the historical estimate label", () => {
    const html = renderToStaticMarkup(
      createElement(StatusBar, { locale: "en-US", wsStatus: "open" }),
    );

    expect(html).toContain("CPU: 38%");
    expect(html).not.toContain("CPU Est.");
  });
});
