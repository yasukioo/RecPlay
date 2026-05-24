import { create } from "zustand";
import type { StatsHistoryPoint, StatsSnapshot } from "../types";

interface StatsStoreState extends StatsSnapshot {
  history: StatsHistoryPoint[];
  replaceSnapshot: (snapshot: Partial<StatsSnapshot>) => void;
}

export const useStatsStore = create<StatsStoreState>((set) => ({
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

  replaceSnapshot: (snapshot) =>
    set((current) => {
      const next = { ...current, ...snapshot };
      const point: StatsHistoryPoint = {
        timestamp: Date.now(),
        total_packets: next.total_packets,
        total_drops: next.total_drops,
        total_throughput_mbps: next.total_throughput_mbps,
        drop_rate: next.drop_rate,
        write_latency_p99_ms: next.write_latency_p99_ms,
        ringbuf_used: next.ringbuf_used,
        ringbuf_capacity: next.ringbuf_capacity,
        disk_queue_bytes: next.disk_queue_bytes,
        cpu_usage_percent: next.cpu_usage_percent,
      };
      return {
        ...next,
        history: [...current.history, point].slice(-30),
      };
    }),
}));
