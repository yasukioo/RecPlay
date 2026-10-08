import { describe, expect, it } from "vitest";

import { parseRealtimeMessage, type RealtimeMessage } from "./useWebSocket";
import { useSessionStore } from "../stores/sessionStore";
import { useStatsStore } from "../stores/statsStore";

describe("parseRealtimeMessage", () => {
  it("parses stats messages", () => {
    const raw = JSON.stringify({ type: "stats", data: { total_packets: 77 } });
    const msg = parseRealtimeMessage(raw);
    expect(msg).toEqual({ type: "stats", data: { total_packets: 77 } });
  });

  it("parses state_changed messages", () => {
    const raw = JSON.stringify({
      type: "state_changed",
      data: { old_state: "Idle", new_state: "Playing" },
    });
    const msg = parseRealtimeMessage(raw);
    expect(msg).toEqual({
      type: "state_changed",
      data: { old_state: "Idle", new_state: "Playing" },
    });
  });

  it("returns null for unknown message types", () => {
    const raw = JSON.stringify({ type: "unknown", data: {} });
    expect(parseRealtimeMessage(raw)).toBeNull();
  });
});

describe("Zustand store integration", () => {
  it("replaceSnapshot updates stats store", () => {
    useStatsStore.getState().replaceSnapshot({
      total_packets: 77,
      total_drops: 2,
      total_throughput_mbps: 5.5,
    });

    const state = useStatsStore.getState();
    expect(state.total_packets).toBe(77);
    expect(state.history.length).toBeGreaterThan(0);
    expect(state.history[state.history.length - 1]!.total_packets).toBe(77);
  });

  it("applyTransition updates session store", () => {
    useSessionStore.getState().applyTransition({
      old_state: "Idle",
      new_state: "Playing",
    });

    const state = useSessionStore.getState();
    expect(state.state).toBe("Playing");
    expect(state.lastTransition?.old_state).toBe("Idle");
  });
});
