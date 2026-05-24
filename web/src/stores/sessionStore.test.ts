import { afterEach, describe, expect, it } from "vitest";

import { useSessionStore } from "./sessionStore";

afterEach(() => {
  useSessionStore.setState({
    state: "Unknown",
    duration_ns: 0,
    position_ns: 0,
    speed: 1,
    loop_start_ns: null,
    loop_end_ns: null,
    config: {
      recordPath: "",
      playbackPath: "",
    },
    lastTransition: null,
  });
});

describe("sessionStore loop region", () => {
  it("stores and clears playback loop ranges", () => {
    useSessionStore.getState().setLoopRegion(100, 500);
    expect(useSessionStore.getState()).toMatchObject({
      loop_start_ns: 100,
      loop_end_ns: 500,
    });

    useSessionStore.getState().clearLoopRegion();
    expect(useSessionStore.getState()).toMatchObject({
      loop_start_ns: null,
      loop_end_ns: null,
    });
  });
});
