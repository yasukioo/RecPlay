import { create } from "zustand";
import type { SessionStateSnapshot, SessionTransition } from "../types";

interface SessionStoreState extends SessionStateSnapshot {
  replaceSnapshot: (snapshot: Omit<SessionStateSnapshot, "lastTransition">) => void;
  applyTransition: (transition: SessionTransition) => void;
}

export const useSessionStore = create<SessionStoreState>((set) => ({
  state: "Unknown",
  duration_ns: 0,
  position_ns: 0,
  speed: 1,
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
}));
