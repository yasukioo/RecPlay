import { describe, expect, it } from "vitest";

import {
  buildSecondaryTrends,
  buildLatencyDistribution,
  getRingBufferPercent,
} from "./monitorDashboardModel";

describe("getRingBufferPercent", () => {
  it("returns a clamped integer percentage", () => {
    expect(getRingBufferPercent(50, 200)).toBe(25);
    expect(getRingBufferPercent(500, 200)).toBe(100);
    expect(getRingBufferPercent(50, 0)).toBe(0);
  });
});

describe("buildLatencyDistribution", () => {
  it("builds a simple low/medium/high distribution from p99 latency", () => {
    expect(buildLatencyDistribution(3)).toEqual([
      { label: "Low", value: 85 },
      { label: "Medium", value: 15 },
      { label: "High", value: 0 },
    ]);

    expect(buildLatencyDistribution(12)).toEqual([
      { label: "Low", value: 25 },
      { label: "Medium", value: 50 },
      { label: "High", value: 25 },
    ]);
  });
});

describe("buildSecondaryTrends", () => {
  it("creates additional time-series panels from richer stats history", () => {
    const trends = buildSecondaryTrends([
      {
        timestamp: 1,
        total_packets: 100,
        total_drops: 2,
        total_throughput_mbps: 5,
        write_latency_p99_ms: 3.5,
        ringbuf_used: 64,
        ringbuf_capacity: 128,
        disk_queue_bytes: 2048,
        drop_rate: 0.02,
      },
      {
        timestamp: 2,
        total_packets: 140,
        total_drops: 3,
        total_throughput_mbps: 5.8,
        write_latency_p99_ms: 6.2,
        ringbuf_used: 96,
        ringbuf_capacity: 128,
        disk_queue_bytes: 4096,
        drop_rate: 0.03,
      },
    ]);

    expect(trends.map((trend) => trend.label)).toEqual([
      "Packets",
      "Drops",
      "Latency",
      "Disk Queue",
    ]);
    expect(trends[0]?.data).toEqual([100, 140]);
    expect(trends[2]?.data).toEqual([3.5, 6.2]);
    expect(trends[3]?.value).toBe("4.0 KB");
  });
});
