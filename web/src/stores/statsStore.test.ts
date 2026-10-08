import { afterEach, describe, expect, it } from "vitest";

import { useStatsStore } from "./statsStore";

afterEach(() => {
  useStatsStore.setState({
    total_throughput_mbps: 0,
    total_packets: 0,
    total_drops: 0,
    drop_rate: 0,
    write_latency_p99_ms: 0,
    ringbuf_used: 0,
    ringbuf_capacity: 0,
    disk_queue_bytes: 0,
    cpu_usage_percent: null,
    history: [],
  });
});

describe("statsStore history", () => {
  it("captures richer snapshot fields in each history point", () => {
    useStatsStore.getState().replaceSnapshot({
      total_packets: 77,
      total_drops: 2,
      total_throughput_mbps: 5.5,
      drop_rate: 0.03,
      write_latency_p99_ms: 7.2,
      ringbuf_used: 192,
      ringbuf_capacity: 256,
      disk_queue_bytes: 8192,
      cpu_usage_percent: 37.5,
    });

    const history = useStatsStore.getState().history;

    expect(history[history.length - 1]).toMatchObject({
      total_packets: 77,
      total_drops: 2,
      total_throughput_mbps: 5.5,
      drop_rate: 0.03,
      write_latency_p99_ms: 7.2,
      ringbuf_used: 192,
      ringbuf_capacity: 256,
      disk_queue_bytes: 8192,
      cpu_usage_percent: 37.5,
    });
  });
});
