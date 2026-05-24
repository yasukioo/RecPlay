import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { TransportBar } from "./TransportBar";

const sessionStoreState = vi.hoisted(() => ({
  state: "Unknown" as "Unknown" | "RecordingPaused",
  duration_ns: 0,
  position_ns: 0,
  speed: 1,
}));

vi.mock("../../stores/sessionStore", () => ({
  useSessionStore: (
    selector: (state: {
      state: typeof sessionStoreState.state;
      duration_ns: number;
      position_ns: number;
      speed: number;
      loop_start_ns: null;
      loop_end_ns: null;
      config: { recordPath: string; playbackPath: string };
      lastTransition: null;
      replaceSnapshot: () => void;
      applyTransition: () => void;
      setLoopRegion: () => void;
      clearLoopRegion: () => void;
      updateConfig: () => void;
    }) => unknown,
  ) =>
    selector({
      ...sessionStoreState,
      loop_start_ns: null,
      loop_end_ns: null,
      config: { recordPath: "", playbackPath: "" },
      lastTransition: null,
      replaceSnapshot: () => undefined,
      applyTransition: () => undefined,
      setLoopRegion: () => undefined,
      clearLoopRegion: () => undefined,
      updateConfig: () => undefined,
    }),
}));

afterEach(() => {
  sessionStoreState.state = "Unknown";
  sessionStoreState.duration_ns = 0;
  sessionStoreState.position_ns = 0;
  sessionStoreState.speed = 1;
});

describe("TransportBar", () => {
  it("renders localized session state labels for Simplified Chinese", () => {
    sessionStoreState.state = "RecordingPaused";
    sessionStoreState.duration_ns = 2_000_000_000;
    sessionStoreState.position_ns = 500_000_000;
    sessionStoreState.speed = 1.5;

    const html = renderToStaticMarkup(
      createElement(TransportBar, { locale: "zh-CN", onError: () => undefined }),
    );

    expect(html).toContain("录制已暂停");
    expect(html).not.toContain("RecordingPaused");
    expect(html).toContain('title="录制"');
    expect(html).toContain('title="播放"');
    expect(html).toContain('title="暂停"');
    expect(html).toContain('title="停止"');
    expect(html).toContain('title="定位"');
  });
});
