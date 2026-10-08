import { create } from "zustand";
import type { SessionConfig, SessionStateSnapshot, SessionTransition } from "../types";

interface SessionStoreState extends SessionStateSnapshot {
  replaceSnapshot: (snapshot: Omit<SessionStateSnapshot, "lastTransition">) => void;
  applyTransition: (transition: SessionTransition) => void;
  setLoopRegion: (startNs: number, endNs: number) => void;
  clearLoopRegion: () => void;
  updateConfig: (patch: Partial<SessionConfig>) => void;
}

export const useSessionStore = create<SessionStoreState>((set) => ({
  state: "Unknown",
  duration_ns: 0,
  position_ns: 0,
  speed: 1,
  loop_start_ns: null,
  loop_end_ns: null,
  config: { recordPath: "", playbackPath: "" },
  lastTransition: null,

  replaceSnapshot: (snapshot) =>
    set((current) => ({
      ...current,
      ...snapshot,
    })),

  applyTransition: (transition) =>
    set((current) => ({
      ...current,
      state: transition.new_state as SessionStoreState["state"],
      lastTransition: transition,
    })),

  setLoopRegion: (startNs, endNs) =>
    set((current) => ({
      ...current,
      loop_start_ns: startNs,
      loop_end_ns: endNs,
    })),

  clearLoopRegion: () =>
    set((current) => ({
      ...current,
      loop_start_ns: null,
      loop_end_ns: null,
    })),

  updateConfig: (patch) =>
    set((current) => ({
      ...current,
      config: { ...current.config, ...patch },
    })),
}));
