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
  history: [],

  replaceSnapshot: (snapshot) =>
    set((current) => {
      const next = { ...current, ...snapshot };
      const point: StatsHistoryPoint = {
        timestamp: Date.now(),
        total_packets: next.total_packets,
        total_drops: next.total_drops,
        total_throughput_mbps: next.total_throughput_mbps,
      };
      return {
        ...next,
        history: [...current.history, point].slice(-30),
      };
    }),
}));
